//
// Created by Zach Lee on 2021/12/5.
//

#pragma once
#include <framework/asset/Asset.h>

namespace sky {

    class ImageAsset : public AssetBase {
    public:
        ImageAsset(const Uuid& id) : AssetBase(id) {}
        ~ImageAsset() = default;

        static constexpr Uuid TYPE = Uuid::CreateFromString("a4a44abf-1b60-438d-b6c5-1690e548d97c");

    private:
        const Uuid& GetType() const override { return TYPE; }
    };

}