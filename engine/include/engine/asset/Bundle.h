//
// Created by Zach Lee on 2021/12/5.
//

#pragma once

#include <framework/asset/Asset.h>

namespace sky {

    class BundleAsset : public AssetBase {
    public:
        BundleAsset(const Uuid& id) : AssetBase(id) {}
        ~BundleAsset() = default;

        static constexpr Uuid TYPE = Uuid::CreateFromString("d9f4b48f-5d35-4d02-a5fe-ac975e663eff");

    private:
        const Uuid& GetType() const override { return TYPE; }
    };

}