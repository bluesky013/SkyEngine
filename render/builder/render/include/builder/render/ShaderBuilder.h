//
// Created by Zach Lee on 2023/2/23.
//

#pragma once

#include <framework/asset/AssetBuilder.h>
#include <builder/shader/ShaderCompiler.h>
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
        std::unique_ptr<sl::ShaderCompiler> compiler;
        std::vector<std::string> extensions = {".vert", ".frag", ".comp"};
    };

}
