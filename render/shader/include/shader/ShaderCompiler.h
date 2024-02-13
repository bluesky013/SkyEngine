//
// Created by Zach Lee on 2023/2/18.
//

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <set>
#include <map>
#include <variant>
#include <unordered_map>
#include <core/environment/Singleton.h>
#include <rhi/Core.h>

namespace sky {

    struct ShaderIncludeContext {
        std::vector<std::string> &searchPaths;
        std::set<std::string> visited;
    };

    struct ShaderResource {
        std::string name;
        rhi::DescriptorType type;
        rhi::ShaderStageFlags visibility;
        uint32_t group;
        uint32_t binding;
        uint32_t count;
        uint32_t size;
    };

    struct ShaderVariable {
        std::string name;
        uint32_t group;
        uint32_t binding;
        uint32_t offset;
        uint32_t size;
    };

    struct ShaderReflection {
        std::vector<ShaderResource> resources;
        std::vector<ShaderVariable> variables;
    };

    struct ShaderBuildResult {
        std::vector<uint32_t> data;
        ShaderReflection reflection;
    };

    enum class ShaderCompileTarget : uint32_t {
        SPIRV,
        DXIL
    };

    struct ShaderDef { bool enable; };
    using MacroValue = std::variant<ShaderDef, uint32_t>;

    class ShaderPreprocessor {
    public:
        ShaderPreprocessor() = default;
        ~ShaderPreprocessor() = default;

        void SetValue(const std::string &key, const MacroValue &val);
        void CalculateHash();
        uint32_t GetHash() const { return hash; }

    private:
        std::map<std::string, MacroValue> values;
        uint32_t hash = 0;
    };
    using ShaderPreprocessorPtr = std::shared_ptr<ShaderPreprocessor>;

    struct ShaderCompileOption {
        ShaderCompileTarget target;
        ShaderPreprocessorPtr preprocessor;
    };

    struct ShaderSourceDesc {
        std::string source;
        std::string entry;
        rhi::ShaderStageFlagBit stage;
    };

    class ShaderCompilerBase {
    public:
        ShaderCompilerBase() = default;
        virtual ~ShaderCompilerBase() = default;

        virtual bool CompileBinary(const ShaderSourceDesc &desc, const ShaderCompileOption &op, ShaderBuildResult &result) = 0;
    };

    class ShaderCompiler : public Singleton<ShaderCompiler> {
    public:
        ShaderCompiler();
        ~ShaderCompiler() override;

        void AddSearchPath(const std::string &path) { searchPaths.emplace_back(path); }

        std::string LoadShader(const std::string &path);

//        void BuildSpirV(const std::string &source,
//                        const std::vector<std::pair<std::string, rhi::ShaderStageFlagBit>> &entries,
//                        std::vector<std::vector<uint32_t>> &out,
//                        ShaderReflection &reflection);

        bool Compile(const ShaderSourceDesc &source, const ShaderCompileOption &option, ShaderBuildResult &result);

    private:
        std::pair<bool, std::string> ProcessShaderSource(const std::string &path);
        std::pair<bool, std::string> ProcessHeaderFile(const std::string &path, ShaderIncludeContext &context, uint32_t depth);

        std::vector<std::string> searchPaths;

        std::unique_ptr<ShaderCompilerBase> compiler;
    };
} // namespace sky
