//
// Created by Zach on 2022/8/8.
//

#pragma once

#include <core/archive/StreamArchive.h>
#include <core/name/Name.h>
#include <core/template/ReferenceObject.h>
#include <core/util/Macros.h>
#include <core/util/Uuid.h>
#include <framework/serialization/BinaryArchive.h>
#include <framework/serialization/JsonArchive.h>

#include <core/async/ThreadPool.h>

#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace sky {

    class AssetBase;
    using AssetPtr = std::shared_ptr<AssetBase>;

    // The completion handle is a shared_future so it can be copied under the lock and awaited outside
    // it (a plain std::future is move-only and would force waiting while holding the lock).
    using AsyncTask = std::pair<TaskNodePtr, std::shared_future<void>>;

    class AssetBase {
    public:
        AssetBase() = default;

        virtual ~AssetBase() = default;

        enum class Status : uint32_t { INITIAL, LOADING, LOADED, FAILED };

        void SetUuid(const Uuid &id)
        {
            uuid = id;
        }
        const Uuid &GetUuid() const
        {
            return uuid;
        }

        void SetType(const Name &val)
        {
            type = val;
        }
        const Name &GetType() const
        {
            return type;
        }

        Status GetStatus() const
        {
            return status.load();
        }
        bool IsLoaded() const
        {
            return status.load() == Status::LOADED;
        }
        const Uuid &GetInstanceId() const
        {
            return instanceId;
        }
        uint64_t GetRevision() const
        {
            return revision;
        }

        void AddDependencies(const Uuid &id);
        void ResetDependencies();
        // Blocks until the asset reaches LOADED or FAILED.
        // MUST NOT be called from the asset executor's worker (loader or cook) pool threads.
        void BlockUntilLoaded() const;

        // ---- bulk payload (build side: opaque bytes appended after the header) ----
        // The per-resource layout (offsets/sizes) lives in the asset's typed header;
        // the bulk is addressed by those (see ReadBulk).
        void AddBulk(const void *data, size_t size);
        // ---- bulk payload (load side: read a byte range of the bulk on demand) ----
        bool HasBulk() const
        {
            return productStream != nullptr;
        }
        std::vector<uint8_t> ReadBulk(uint64_t offset, uint64_t size) const;

        virtual const uint8_t *GetData() const = 0;

        AsyncTask GetAsyncTask() const
        {
            std::lock_guard<std::mutex> lock(asyncMutex);
            return asyncTask;
        }

        TaskNodePtr GetAsyncNode() const
        {
            std::lock_guard<std::mutex> lock(asyncMutex);
            return asyncTask.first;
        }

        // Copy the wait handle (safe to hold/await after the caller releases any external lock).
        std::shared_future<void> GetWaitHandle() const
        {
            std::lock_guard<std::mutex> lock(asyncMutex);
            return asyncTask.second;
        }

        void SetAsyncTask(TaskNodePtr node, std::shared_future<void> handle)
        {
            std::lock_guard<std::mutex> lock(asyncMutex);
            asyncTask.first  = std::move(node);
            asyncTask.second = std::move(handle);
        }

        void SetWaitHandle(std::shared_future<void> handle)
        {
            std::lock_guard<std::mutex> lock(asyncMutex);
            asyncTask.second = std::move(handle);
        }

    protected:
        friend class AssetManager;

        Uuid                  instanceId = Uuid::Create();
        Uuid                  uuid;
        Name                  type;
        std::vector<Uuid>     dependencies;
        std::vector<AssetPtr> depAssets;

        // build side
        std::vector<uint8_t> bulk;
        // load side
        uint64_t                  bulkStart = 0;
        mutable IStreamArchivePtr productStream;
        uint64_t                  revision = 1;
        mutable std::mutex        bulkMutex;

        std::atomic<Status> status = Status::INITIAL;
        mutable std::mutex  asyncMutex;
        AsyncTask           asyncTask;
    };

    enum class SerializeType : uint8_t { JSON, BIN };

    template <typename T>
    struct AssetTraits {
        using DataType                                = std::vector<uint8_t>;
        static constexpr SerializeType SERIALIZE_TYPE = SerializeType::JSON;
    };

    template <typename T>
    class Asset : public AssetBase {
    public:
        Asset() = default;

        ~Asset() override = default;

        using DataType = typename AssetTraits<T>::DataType;

        const DataType &Data() const
        {
            return data;
        }
        DataType &Data()
        {
            return data;
        }

        template <typename Func>
        const CounterPtr<T> &GetOrCreateResource(Func &&createFn)
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (cacheObject) {
                return cacheObject;
            }

            cacheObject = createFn(*this);
            return cacheObject;
        }

        const uint8_t *GetData() const override
        {
            return reinterpret_cast<const uint8_t *>(&data);
        }

    private:
        friend class AssetManager;

        DataType data;

        mutable std::mutex mutex;
        CounterPtr<T>      cacheObject;
    };

    class AssetHandlerBase {
    public:
        AssetHandlerBase() = default;

        virtual ~AssetHandlerBase() = default;

        SKY_DISABLE_COPY(AssetHandlerBase)

        virtual std::shared_ptr<AssetBase> CreateAsset() = 0;

        virtual bool Load(IStreamArchive &archive, const std::shared_ptr<AssetBase> &asset) = 0;

        virtual bool SaveChecked(OStreamArchive &, const std::shared_ptr<AssetBase> &) = 0;
    };

    template <typename T>
    class AssetHandler : public AssetHandlerBase {
    public:
        AssetHandler() = default;

        ~AssetHandler() override = default;

        using DataType = typename AssetTraits<T>::DataType;

        static constexpr SerializeType SERIALIZE_TYPE = AssetTraits<T>::SERIALIZE_TYPE;

        std::shared_ptr<AssetBase> CreateAsset() override
        {
            return std::make_shared<Asset<T>>();
        }

        bool Load(IStreamArchive &archive, const std::shared_ptr<AssetBase> &assetBase) override
        {
            auto      asset     = std::static_pointer_cast<Asset<T>>(assetBase);
            DataType &assetData = asset->Data();
            if (SERIALIZE_TYPE == SerializeType::JSON) {
                JsonInputArchive jArchive(archive);
                jArchive.LoadValueById(&assetData, TypeInfo<DataType>::RegisteredId());
            } else if (SERIALIZE_TYPE == SerializeType::BIN) {
                BinaryInputArchive bArchive(archive);
                bArchive.LoadObject(&assetData, TypeInfo<DataType>::RegisteredId());
                if (!bArchive.IsValid()) {
                    return false;
                }
            }
            if constexpr (requires { assetData.IsLoadValid(); }) {
                return assetData.IsLoadValid();
            }
            return true;
        }

        bool SaveChecked(OStreamArchive &archive, const std::shared_ptr<AssetBase> &assetBase) override
        {
            if constexpr (SERIALIZE_TYPE != SerializeType::BIN) {
                return false;
            }
            BinaryOutputArchive binary(archive);
            binary.SaveObject(assetBase->GetData(), TypeInfo<DataType>::RegisteredId());
            return binary.IsValid() && archive.GetStream().good();
        }

    protected:
        virtual void LoadBinary(BinaryInputArchive &archive)
        {
        }
        virtual void SaveBinary(BinaryOutputArchive &archive)
        {
        }

        virtual void LoadJson(JsonInputArchive &archive)
        {
        }
        virtual void SaveJson(JsonOutputArchive &archive)
        {
        }
    };
} // namespace sky
