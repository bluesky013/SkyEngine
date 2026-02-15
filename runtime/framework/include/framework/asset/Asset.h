//
// Created by Zach on 2022/8/8.
//

#pragma once

#include <core/util/Uuid.h>
#include <core/util/Macros.h>
#include <core/name/Name.h>
#include <core/archive/StreamArchive.h>
#include <core/async/ThreadPool.h>
#include <framework/serialization/JsonArchive.h>
#include <framework/serialization/BinaryArchive.h>

#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace sky {

    class AssetBase;
    using AssetPtr = std::shared_ptr<AssetBase>;

    using AsyncTask = std::pair<AsyncTaskHandle, std::future<void>>;

    class AssetBase {
    public:
        AssetBase() = default;

        virtual ~AssetBase() = default;

        enum class Status : uint32_t { INITIAL, LOADING, LOADED, FAILED };

        void SetUuid(const Uuid &id) { uuid = id; }
        const Uuid &GetUuid() const { return uuid; }

        void SetType(const Name &val) { type = val; }
        const Name &GetType() const { return type; }
        
        Status GetStatus() const { return status.load(); }
        bool IsLoaded() const { return status.load() == Status::LOADED; }

        void AddDependencies(const Uuid &id);
        void ResetDependencies();
        void BlockUntilLoaded() const;

        virtual const uint8_t *GetData() const = 0;

        const AsyncTask &GetAsyncTask() const { return asyncTask; }
    protected:
        friend class AssetManager;
        friend class AssetManager;

        Uuid                  uuid;
        Name                  type;
        std::vector<Uuid>     dependencies;
        std::vector<AssetPtr> depAssets;

        std::atomic<Status> status = Status::INITIAL;
        AsyncTask           asyncTask;
    };

    enum class SerializeType : uint8_t { JSON, BIN };

    template <typename T>
    struct AssetTraits {
        using DataType = std::vector<uint8_t>;
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

        const uint8_t *GetData() const override { return reinterpret_cast<const uint8_t *>(&data); }
    private:
        friend class AssetManager;

        DataType data;
    };

    class AssetHandlerBase {
    public:
        AssetHandlerBase() = default;

        virtual ~AssetHandlerBase() = default;

        SKY_DISABLE_COPY(AssetHandlerBase)

        virtual std::shared_ptr<AssetBase> CreateAsset() = 0;

        virtual bool Load(IStreamArchive &archive, const std::shared_ptr<AssetBase> &asset) = 0;

        virtual void Save(OStreamArchive &archive, const std::shared_ptr<AssetBase> &data) = 0;
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
            auto     asset = std::static_pointer_cast<Asset<T>>(assetBase);
            DataType &assetData = asset->Data();
            if (SERIALIZE_TYPE == SerializeType::JSON) {
                JsonInputArchive jArchive(archive);
                jArchive.LoadValueById(&assetData, TypeInfo<DataType>::RegisteredId());
            } else if (SERIALIZE_TYPE == SerializeType::BIN) {
                BinaryInputArchive bArchive(archive);
                bArchive.LoadObject(&assetData, TypeInfo<DataType>::RegisteredId());
            }
            return true;
        }

        void Save(OStreamArchive &archive, const std::shared_ptr<AssetBase> &assetBase) override
        {
            auto          asset     = std::static_pointer_cast<Asset<T>>(assetBase);
            auto         &assetData = asset->Data();

            if (SERIALIZE_TYPE == SerializeType::JSON) {
//                JsonOutputArchive archive(file);
//                archive.SaveValueObject(assetBase->GetData(), TypeInfo<DataType>::RegisteredId());
            } else if (SERIALIZE_TYPE == SerializeType::BIN) {
                BinaryOutputArchive bArchive(archive);
                bArchive.SaveObject(assetBase->GetData(), TypeInfo<DataType>::RegisteredId());
            }
        }

    protected:
        virtual void LoadBinary(BinaryInputArchive &archive) {}
        virtual void SaveBinary(BinaryOutputArchive &archive) {}

        virtual void LoadJson(JsonInputArchive &archive) {}
        virtual void SaveJson(JsonOutputArchive &archive) {}
    };
} // names