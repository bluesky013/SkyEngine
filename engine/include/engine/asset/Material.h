//
// Created by Zach Lee on 2021/12/5.
//

#pragma once

#include <framework/asset/Asset.h>
#include <framework/asset/Resource.h>

namespace sky {

    class MaterialAsset : public AssetBase {
    public:
        MaterialAsset(const Uuid& id) : AssetBase(id) {}
        ~MaterialAsset() = default;

        static constexpr Uuid TYPE = Uuid::CreateFromString("9c158e38-033c-464d-be3d-619ecd4e5954");

    private:
        const Uuid& GetType() const override { return TYPE; }
    };

    class MaterialHandler : public AssetHandlerBase {
    public:
        MaterialHandler() = default;
        ~MaterialHandler() = default;

        AssetPtr Create(const Uuid& id);

        AssetPtr Load(const std::string&);
    };

    class Material : public ResourceBase {
    public:
        Material(const Uuid& id) : ResourceBase(id) {}
        ~Material() = default;
    };

}