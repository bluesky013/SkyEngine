//
// Created by Zach Lee on 2023/2/23.
//

#pragma once

#include <framework/asset/AssetBuilder.h>
#include <string_view>

namespace sky::builder {

    class MaterialBuilder : public AssetBuilder {
    public:
        MaterialBuilder() = default;
        ~MaterialBuilder() override = default;

        static constexpr std::string_view KEY = "GFX_MATERIAL";

        void Request(const BuildRequest &build, BuildResult &result) override;
    };

}
