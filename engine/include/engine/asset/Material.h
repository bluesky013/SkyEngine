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

        static constexpr Uuid TYPE = Uuid::CreateFromString("28aca595-503d-4156-b17c-a7c65bde6bdf");

    private:
        const Uuid& GetType() const override { return TYPE; }
    };

    class MaterialInstance : public AssetInstanceBase {
    public:
        MaterialInstance() = default;
        ~MaterialInstance() = default;

        static constexpr Uuid TYPE = Uuid::CreateFromString("9c158e38-033c-464d-be3d-619ecd4e5954");

    private:
        const Uuid& GetType() const override { return TYPE; }
        Material* parent = nullptr;
    };

}