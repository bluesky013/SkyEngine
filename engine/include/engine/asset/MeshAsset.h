//
// Created by Zach Lee on 2021/12/5.
//

#pragma once

#include <framework/asset/Asset.h>
#include <framework/asset/Resource.h>

namespace sky {

    class MeshAsset : public AssetBase {
    public:
        MeshAsset(const Uuid& id) : AssetBase(id) {}
        ~MeshAsset() = default;

        static constexpr Uuid TYPE = Uuid::CreateFromString("9f7c599a-0073-4ff5-8136-f551d1a1a371");

    private:
        const Uuid& GetType() const override { return TYPE; }
    };

    class Mesh : public ResourceBase {
    public:
        Mesh(const Uuid& id) : ResourceBase(id) {}
        ~Mesh() = default;
    };

}