//
// Created by Zach Lee on 2021/12/5.
//

#pragma once

#include <framework/asset/Asset.h>

namespace sky {

    class ModelAsset : public AssetBase {
    public:
        ModelAsset(const Uuid& id) : AssetBase(id) {}
        ~ModelAsset() = default;

        static constexpr Uuid TYPE = Uuid::CreateFromString("9f7c599a-0073-4ff5-8136-f551d1a1a371");

    private:
        const Uuid& GetType() const override { return TYPE; }
    };

}