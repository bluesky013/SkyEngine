//
// Created by Zach Lee on 2021/12/5.
//

#pragma once
#include <framework/asset/Asset.h>

namespace sky {

    class Image : public AssetInstanceBase {
    public:
        Image() = default;
        ~Image() = default;

    private:
        uint32_t GetType() const override { return 0; }
    };

}