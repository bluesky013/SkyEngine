//
// Created by Zach Lee on 2021/12/5.
//

#pragma once

#include <framework/asset/Asset.h>

namespace sky {

    class Material : public AssetInstanceBase {
    public:
        Material() = default;
        ~Material() = default;

    private:
        uint32_t GetType() const override { return 0; }
    };

    class MaterialInstance : public AssetInstanceBase {
    public:
        MaterialInstance() = default;
        ~MaterialInstance() = default;

    private:
        uint32_t GetType() const override { return 0; }
        Material* parent = nullptr;
    };

}