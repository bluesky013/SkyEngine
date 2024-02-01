//
// Created by Zach Lee on 2023/2/18.
//

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <set>
#include <unordered_map>
#include <core/environment/Singleton.h>
#include <rhi/Core.h>

namespace sky::sl {

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
        uint32_t size;
    };

    struct ShaderVariable {
        std::string name;
        uint32_t group;
        uint32_t binding;
        uint32_t offset;
    };

    struct ShaderReflection {
        std::vector<ShaderResource> resources;
        std::vector<ShaderVariable> variables;
    };

    class ShaderCompiler : public Singleton<ShaderCompiler> {
    public:
        ShaderCompiler();
        ~ShaderCompiler() override;

        std::string LoadShader(const std::string &path);
        void AddSearchPath(const std::string &path) { searchPaths.emplace_back(path); }

        void BuildSpirV(const std::string &source,
                        const std::vector<std::pair<std::string, rhi::ShaderStageFlagBit>> &entries,
                        std::vector<std::vector<uint32_t>> &out,
                        ShaderReflection &reflection);

        void BuildDXIL(const std::string &source);

    private:
        std::pair<bool, std::string> ProcessShaderSource(const std::string &path);
        std::pair<bool, std::string> ProcessHeaderFile(const std::string &path, ShaderIncludeContext &context, uint32_t depth);

        std::vector<std::string> searchPaths;
    };
} // namespace sky
