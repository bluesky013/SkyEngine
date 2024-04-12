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

        const std::vector<std::string> &GetExtensions() const override { return extensions; }

    private:
        void BuildMaterial(const BuildRequest &build, BuildResult &result);
        void BuildMaterialInstance(const BuildRequest &build, BuildResult &result);

        std::vector<std::string> extensions = {".mat", ".mati"};
    };

}
