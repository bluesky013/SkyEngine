//
// Created by Zach on 2022/8/8.
//

#pragma once

#include <core/util/Uuid.h>
#include <core/jobsystem/JobSystem.h>
#include <cereal/archives/json.hpp>
#include <cereal/archives/binary.hpp>
#include <cereal/archives/xml.hpp>
#include <memory>
#include <vector>
#include <string>
#include <mutex>

namespace sky {

    class AssetBase {
    public:
        AssetBase() = default;

        virtual ~AssetBase() = default;

        enum class Status {
            INITIAL,
            LOADING,
            LOADED
        };

        void SetUuid(const Uuid &id);

        const Uuid &GetUuid() const;

        Status GetStatus() const;

        void BlockUtilLoaded() {
            if (future.valid()) {
                future.wait();
            }
        }

    protected:
        friend class AssetManager;

        Uuid uuid;
        Status status = Status::INITIAL;
        tf::Future<void> future;
    };

    using AssetPtr = std::shared_ptr<AssetBase>;

    enum class SerializeType : uint8_t {
        JSON,
        BIN,
        XML
    };

    template<typename T>
    struct AssetTraits {
        using DataType = std::vector<uint8_t>;
        static constexpr Uuid ASSET_TYPE;
        static constexpr SerializeType SERIALIZE_TYPE = SerializeType::JSON;

        static std::shared_ptr<T> CreateFromData(const DataType& data)
        {
            return nullptr;
        }
    };

    template<typename T>
    class Asset : public AssetBase {
    public:
        Asset() = default;

        ~Asset() override = default;

        using DataType = typename AssetTraits<T>::DataType;

        void SetData(const DataType &input) {
            data = input;
        }

        void SetData(DataType &&input) {
            data = std::move(input);
        }

        std::shared_ptr<T> CreateInstance(bool useDefault = true) {
            std::shared_ptr<T> res;
            if (useDefault) {
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

        const DataType &Data() const {
            return data;
        }

        DataType &Data()
        {
            return data;
        }

        template<class Archiver>
        void serialize(Archiver &ar) {
            ar(uuid);
        }

    private:
        friend class AssetManager;

        DataType data;

        std::mutex mutex;
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

        static std::string GetRealPath(const std::string &path);
    };

    template<typename T>
    class AssetHandler : public AssetHandlerBase {
    public:
        AssetHandler() = default;

        ~AssetHandler() override = default;

        using DataType = typename AssetTraits<T>::DataType;
        static constexpr SerializeType SERIALIZE_TYPE = AssetTraits<T>::SERIALIZE_TYPE;

        std::shared_ptr<AssetBase> CreateAsset() override {
            return std::make_shared<Asset<T>>();
        }

        void LoadFromPath(const std::string &path, const std::shared_ptr<AssetBase> &assetBase) override {
            std::ifstream file(GetRealPath(path), std::ios::binary);
            if (!file.is_open()) {
                return;
            }

            auto asset = std::static_pointer_cast<Asset<T>>(assetBase);
            DataType assetData;
            if (SERIALIZE_TYPE == SerializeType::JSON) {
                cereal::JSONInputArchive archive(file);
                archive >> assetData;
            } else if (SERIALIZE_TYPE == SerializeType::BIN) {
                cereal::BinaryInputArchive archive(file);
                archive >> assetData;
            } else if (SERIALIZE_TYPE == SerializeType::XML) {
                cereal::XMLInputArchive archive(file);
                archive >> assetData;
            }
            asset->SetData(std::move(assetData));
        }

        void SaveToPath(const std::string &path, const std::shared_ptr<AssetBase> &assetBase) override {
            auto asset = std::static_pointer_cast<Asset<T>>(assetBase);
            auto &assetData = asset->Data();
            std::ofstream file(path, std::ios::binary);
            if (!file.is_open()) {
                return;
            }
            if (SERIALIZE_TYPE == SerializeType::JSON) {
                cereal::JSONOutputArchive archive(file);
                archive << assetData;
            } else if (SERIALIZE_TYPE == SerializeType::BIN) {
                cereal::BinaryOutputArchive archive(file);
                archive << assetData;
            } else if (SERIALIZE_TYPE == SerializeType::XML) {
                cereal::XMLOutputArchive archive(file);
                archive << assetData;
            }
        }
    };
}