//
// Created by Zach Lee on 2023/2/18.
//

#pragma once

#include <string>
#include <vector>
#include <memory>

namespace sky::builder {

    enum class ShaderType {
        VS,
        FS,
        CS
    };

    class ShaderCompiler {
    public:
        ShaderCompiler() = default;
        ~ShaderCompiler() = default;

        struct Option {
            std::string output;
            ShaderType type;
        };

        static void BuildSpirV(const std::string &path, ShaderType type, std::vector<uint32_t> &out);
        static std::string BuildGLES(const std::vector<uint32_t> &spv);

        static void CompileShader(const std::string &path, const Option &option);
    };
}
