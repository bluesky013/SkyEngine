//
// Created by Zach Lee on 2023/2/18.
//

#pragma once

#include <framework/asset/AssetBuilder.h>
#include <string>
#include <vector>
#include <memory>

namespace sky::builder {

    enum class ShaderType {
        VS,
        FS,
        CS
    };

    class ShaderCompiler : public AssetBuilder {
    public:
        ShaderCompiler() = default;
        ~ShaderCompiler() = default;

        struct Option {
            std::string output;
            ShaderType type;
        };

        static void BuildSpirV(const std::string &path, ShaderType type, std::vector<uint32_t> &out);
        static void CompileShader(const std::string &path, const Option &option);

        void Request(BuildRequest &build) override;
    };
}