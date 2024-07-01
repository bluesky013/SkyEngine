//
// Created by Zach on 2022/8/8.
//

#pragma once

#include <core/jobsystem/JobSystem.h>
#include <core/util/Uuid.h>
#include <core/archive/IArchive.h>
#include <framework/serialization/JsonArchive.h>
#include <framework/serialization/BinaryArchive.h>

#include <taskflow/taskflow.hpp>

#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace sky {

    class AssetBase;
    using AssetPtr = std::shared_ptr<AssetBase>;

    using AsyncTask = std::pair<tf::AsyncTask, std::future<void>>;

    class AssetBase {
    public:
        AssetBase() = default;

        virtual ~AssetBase() = default;

        enum class Status : uint32_t { INITIAL, LOADING, LOADED, FAILED };

        void SetUuid(const Uuid &id) { uuid = id; }
        const Uuid &GetUuid() const { return uuid; }

        void SetTypeId(const Uuid &id) { typeId = id; }
        const Uuid &GetTypeId() const { return typeId; }

        void SetType(const std::string &val) { type = val; }
        const std::string &GetType() const { return type; }

        void SetName(const std::string &name) { markedName = name; }
        const std::string &GetName() const { return markedName; }
        
        Status GetStatus() const { return status.load(); }
        bool IsLoaded() const { return status.load() == Status::LOADED; }

        void AddDependencies(const Uuid &id);
        void BlockUntilLoaded() const;

        virtual const uint8_t *GetData() const = 0;
    protected:
        friend class AssetManager;
        friend class AssetManager;

        Uuid                  uuid;
        Uuid                  typeId; // TODO
        std::string           type;
        std::vector<Uuid>     dependencies;
        std::vector<AssetPtr> depAssets;
        std::string           markedName;

        std::atomic<Status> status = Status::INITIAL;
        AsyncTask           asyncTask;
    };

    enum class SerializeType : uint8_t { JSON, BIN };

    template <typename T>
    struct AssetTraits {
        using DataType = std::vector<uint8_t>;
        static constexpr SerializeType SERIALIZE_TYPE = SerializeType::JSON;

        static std::shared_ptr<T> CreateFromData(const DataType &data)
        {
            return nullptr;
        }
    };

    template <typename T>
    class Asset : public AssetBase {
    public:
        Asset() = default;

        ~Asset() override = default;

        using DataType = typename AssetTraits<T>::DataType;

        std::shared_ptr<T> CreateInstance(bool useDefault = true)
        {
            if (useDefault) {
                std::shared_ptr<T> res;
                std::lock_guard<std::mutex> lock(mutex);
                res = defaultInstance.lock();
                if (res) {
                    return res;
                }
                res = AssetTraits<T>::CreateFromData(data);
                defaultInstance = res;
                return res;
            }
            return AssetTraits<T>::CreateFromData(data);
        }

        template <typename U>
        std::shared_ptr<U> CreateInstanceAs(bool useDefault = true)
        {
            return std::static_pointer_cast<U>(CreateInstance(useDefault));
        }

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

        std::mutex       mutex;
        std::weak_ptr<T> defaultInstance;
    };

    class AssetHandlerBase {
    public:
        AssetHandlerBase() = default;

        virtual ~AssetHandlerBase() = default;

        SKY_DISABLE_COPY(AssetHandlerBase)

        virtual std::shared_ptr<AssetBase> CreateAsset() = 0;

        virtual bool Load(IInputArchive &archive, const std::shared_ptr<AssetBase> &asset) = 0;

        virtual void Save(IOutputArchive &archive, const std::shared_ptr<AssetBase> &data) = 0;
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

        bool Load(IInputArchive &archive, const std::shared_ptr<AssetBase> &assetBase) override
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

        void Save(IOutputArchive &archive, const std::shared_ptr<AssetBase> &assetBase) override
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