//
// Created by Zach Lee on 2023/2/23.
//

#pragma once

#include <framework/asset/AssetBuilder.h>
#include <string_view>

namespace sky::builder {

    class ShaderBuilder : public AssetBuilder {
    public:
        ShaderBuilder() = default;
        ~ShaderBuilder() override = default;

        static constexpr std::string_view KEY = "GFX_SHADER";

        void Request(const BuildRequest &build, BuildResult &result) override;
        void LoadConfig(const std::string &path) override;
        const std::vector<std::string> &GetExtensions() const override { return extensions; }
    private:
        std::vector<std::string> extensions = {".hlsl"};
    };

}
