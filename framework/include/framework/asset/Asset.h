//
// Created by Zach Lee on 2021/12/3.
//

#pragma once

#include <core/util/Rtti.h>
#include <core/util/Uuid.h>
#include <core/template/ReferenceObject.h>
#include <framework/asset/Resource.h>
#include <cereal/archives/binary.hpp>
#include <fstream>

namespace sky {

    class AssetBase : public RefObject<AssetBase> {
    public:
        AssetBase(const Uuid& id) : uuid(id) {}
        virtual ~AssetBase() = default;

        enum class Status : uint8_t {
            UNLOAD,
            LOADING,
            LOADED
        };

        bool IsReady() const { return status == Status::LOADED; }

        const Uuid& GetId() const { return uuid; }

        virtual const Uuid& GetType() const = 0;

        virtual ResourceInstance CreateInstance(const Uuid&)
        {
            return ResourceInstance {};
        }

    protected:
        friend class AssetManager;

        void OnExpire() override;

        Uuid uuid;
        Status status = Status::UNLOAD;
        std::string path;
    };

    using AssetPtr = CounterPtr<AssetBase>;

    class AssetHandlerBase {
    public:
        AssetHandlerBase() = default;
        virtual ~AssetHandlerBase() = default;

        virtual AssetBase* Create(const Uuid& id) = 0;

        virtual AssetBase* Load(const std::string& path) = 0;

        virtual void SaveAsset(AssetBase*, const std::string& path) = 0;

    protected:
    };

    template <class Asset>
    class AssetHandler : public AssetHandlerBase {
    public:
        AssetBase* Create(const Uuid& id) override
        {
            return new Asset(id);
        }

        AssetBase* Load(const std::string& path) override
        {
            std::ifstream is(path, std::ios::binary);
            cereal::BinaryInputArchive archive(is);

            auto asset = new Asset(Uuid::Create());
            archive(*asset);
            return asset;
        }

        void SaveAsset(AssetBase* ptr, const std::string& path) override
        {
            std::ofstream os(path, std::ios::binary);
            cereal::BinaryOutputArchive archive(os);

            auto& asset = *static_cast<Asset*>(ptr);
            archive(asset);
        }
    };

    template <typename T>
    CounterPtr<T> Cast(AssetPtr asset)
    {
        return CounterPtr<T>(static_cast<T*>(asset.Get()));
    }
}
