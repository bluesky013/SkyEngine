//
// Created by Zach on 2022/8/8.
//

#pragma once

#include <core/jobsystem/JobSystem.h>
#include <core/util/Uuid.h>
#include <framework/serialization/JsonArchive.h>
#include <framework/serialization/BinaryArchive.h>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace sky {

    class AssetBase {
    public:
        AssetBase() = default;

        virtual ~AssetBase() = default;

        enum class Status { INITIAL, LOADING, LOADED };

        void SetUuid(const Uuid &id);
        const Uuid &GetUuid() const { return uuid; }

        virtual const Uuid &GetType() const = 0;
        Status GetStatus() const { return status; }

        virtual const uint8_t *GetData() const = 0;

        void BlockUtilLoaded()
        {
            if (future.valid()) {
                future.wait();
            }
        }

    protected:
        friend class AssetManager;
        friend class AssetManager;

        Uuid             uuid;
        Status           status = Status::INITIAL;
        tf::Future<void> future;
    };

    using AssetPtr = std::shared_ptr<AssetBase>;

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

        const Uuid &GetType() const override
        {
            return AssetTraits<T>::ASSET_TYPE;
        }

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

        virtual void LoadFromPath(const std::string &path, const std::shared_ptr<AssetBase> &asset) = 0;

        virtual void SaveToPath(const std::string &path, const std::shared_ptr<AssetBase> &data) = 0;
    };

    template <typename T>
    class AssetHandler : public AssetHandlerBase {
    public:
        AssetHandler() = default;

        ~AssetHandler() override = default;

        using DataType                                = typename AssetTraits<T>::DataType;
        static constexpr SerializeType SERIALIZE_TYPE = AssetTraits<T>::SERIALIZE_TYPE;

        std::shared_ptr<AssetBase> CreateAsset() override
        {
            return std::make_shared<Asset<T>>();
        }

        void LoadFromPath(const std::string &path, const std::shared_ptr<AssetBase> &assetBase) override
        {
            std::ifstream file(path, std::ios::binary);
            if (!file.is_open()) {
                return;
            }

            auto     asset = std::static_pointer_cast<Asset<T>>(assetBase);
            DataType &assetData = asset->Data();
            if (SERIALIZE_TYPE == SerializeType::JSON) {
                JsonInputArchive archive(file);
            } else if (SERIALIZE_TYPE == SerializeType::BIN) {
                BinaryInputArchive archive(file);
                archive.LoadObject(&assetData, TypeInfo<DataType>::RegisteredId());
            }
        }

        void SaveToPath(const std::string &path, const std::shared_ptr<AssetBase> &assetBase) override
        {
            auto          asset     = std::static_pointer_cast<Asset<T>>(assetBase);
            auto         &assetData = asset->Data();
            std::ofstream file(path, std::ios::binary);
            if (!file.is_open()) {
                return;
            }
            if (SERIALIZE_TYPE == SerializeType::JSON) {
                JsonOutputArchive archive(file);
                archive.SaveValueObject(assetBase->GetData(), TypeInfo<DataType>::RegisteredId());
            } else if (SERIALIZE_TYPE == SerializeType::BIN) {
                BinaryOutputArchive archive(file);
                archive.SaveObject(assetBase->GetData(), TypeInfo<DataType>::RegisteredId());
            }
        }
    };
} // namespace sky
