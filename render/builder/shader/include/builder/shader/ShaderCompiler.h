//
// Created by Zach Lee on 2023/2/18.
//

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

namespace sky::sl {

    enum class ShaderType {
        VS,
        FS,
        CS
    };

    enum class Language {
        GLSL,
        HLSL
    };

    class ShaderCompiler {
    public:
        ShaderCompiler();
        ~ShaderCompiler();

        class Includer {
        public:
            Includer() = default;
            virtual ~Includer() = default;
        };

        struct Option {
            std::string output;
            ShaderType type = ShaderType::VS;
            Language language = Language::GLSL;
        };

        void AddEngineIncludePath(const std::string &path);

        bool BuildSpirV(const std::string &path, std::vector<uint32_t> &out, const Option &option);
//        void CompileShader(const std::string &path, const Option &option);

//        static std::string BuildGLES(const std::vector<uint32_t> &spv, const Option &option = {});
//        static std::string BuildMSL(const std::vector<uint32_t> &spv, const Option &option = {});

    private:
        std::unique_ptr<Includer> includer;
    };
}
