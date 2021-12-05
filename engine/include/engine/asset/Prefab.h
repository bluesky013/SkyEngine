//
// Created by Zach Lee on 2021/12/5.
//

#pragma once

#include <framework/asset/Asset.h>

namespace sky {

    class Prefab : public AssetInstanceBase {
    public:
        Prefab() = default;
        ~Prefab() = default;

    private:
        uint32_t GetType() const override { return 0; }
    };

}