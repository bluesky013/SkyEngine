//
// Created by Zach Lee on 2021/12/5.
//

#pragma once

#include <framework/asset/Asset.h>
#include <framework/asset/Resource.h>

namespace sky {

    class ModelAsset : public AssetBase {
    public:
        ModelAsset(const Uuid& id) : AssetBase(id) {}
        ~ModelAsset() = default;

        static constexpr Uuid TYPE = Uuid::CreateFromString("9f7c599a-0073-4ff5-8136-f551d1a1a371");

    private:
        const Uuid& GetType() const override { return TYPE; }
    };

    class ModelHandler : public AssetHandlerBase {
    public:
        ModelHandler() = default;
        ~ModelHandler() = default;

        AssetPtr Create(const Uuid& id);

        AssetPtr Load(const std::string&);
    };

    class Model : public ResourceBase {
    public:
        Model(const Uuid& id) : ResourceBase(id) {}
        ~Model() = default;
    };

}