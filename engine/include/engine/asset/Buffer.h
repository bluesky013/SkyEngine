//
// Created by Zach Lee on 2021/12/5.
//

#pragma once

#include <framework/asset/Asset.h>

namespace sky {

    class Buffer : public AssetInstanceBase {
    public:
        Buffer() = default;
        ~Buffer() = default;

    private:
        uint32_t GetType() const override { return 0; }
    };

}