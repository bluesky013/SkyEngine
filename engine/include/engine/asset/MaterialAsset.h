//
// Created by Zach Lee on 2021/12/5.
//

#pragma once

#include <framework/asset/Asset.h>
#include <framework/asset/Resource.h>
#include <framework/serialization/Any.h>
#include <unordered_map>
#include <string>

namespace sky {
    class MaterialInstance;

    class MaterialAsset : public AssetBase {
    public:
        MaterialAsset(const Uuid& id) : AssetBase(id) {}
        ~MaterialAsset() = default;

        static constexpr Uuid TYPE = Uuid::CreateFromString("9c158e38-033c-464d-be3d-619ecd4e5954");

    private:
        const Uuid& GetType() const override { return TYPE; }
    };

    struct MaterialLayout {

    };

    class Material : public ResourceBase {
    public:
        Material(const Uuid& id) : ResourceBase(id) {}
        ~Material() = default;

    private:
        MaterialLayout layout;
        std::unordered_map<std::string, Any> properties;
    };
    using MaterialPtr = CounterPtr<Material>;

}