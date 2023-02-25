//
// Created by Zach Lee on 2023/2/23.
//

#pragma once

#include <framework/asset/AssetBuilder.h>

namespace sky::builder {

    class MaterialBuilder : public AssetBuilder {
    public:
        MaterialBuilder() = default;
        ~MaterialBuilder() = default;

        void Request(const BuildRequest &build, BuildResult &result) override {}
    };

}