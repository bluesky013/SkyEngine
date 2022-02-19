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

    struct MaterialData {

        template <class Archive>
        void serialize(Archive& ar)
        {
        }
    };


    class MaterialAsset : public AssetBase {
    public:
        MaterialAsset(const Uuid& id) : AssetBase(id) {}
        ~MaterialAsset() = default;

        static constexpr Uuid TYPE = Uuid::CreateFromString("9c158e38-033c-464d-be3d-619ecd4e5954");

        template<class Archive>
        void load(Archive& ar)
        {
        }

        template<class Archive>
        void save(Archive& ar) const
        {
        }

    private:
        const Uuid& GetType() const override { return TYPE; }
    };

    class Material : public ResourceBase {
    public:
        Material(const Uuid& id) : ResourceBase(id) {}
        ~Material() = default;

    private:
        std::unordered_map<std::string, Any> properties;
    };
    using MaterialPtr = CounterPtr<Material>;

}