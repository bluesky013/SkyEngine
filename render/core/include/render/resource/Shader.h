//
// Created by Zach Lee on 2023/8/31.
//

#pragma once

#include <utility>
#include <vector>
#include <memory>
#include <map>
#include <variant>
#include <rhi/Device.h>
#include <core/util/Uuid.h>

#include <render/resource/ResourceGroup.h>
#include <shader/ShaderCompiler.h>

namespace sky {
    class Program {
    public:
        Program() = default;
        ~Program() = default;

        RDResourceLayoutPtr RequestLayout(uint32_t index) const;
        const rhi::PipelineLayoutPtr &GetPipelineLayout() const { return pipelineLayout; }

        void AddShader(const rhi::ShaderPtr &shader) { shaders.emplace_back(shader); }

    private:
        ShaderReflection reflection;
        rhi::PipelineLayoutPtr pipelineLayout;
        std::vector<rhi::ShaderPtr> shaders;
    };
    using RDProgramPtr = std::shared_ptr<Program>;

    class ShaderCollection {
    public:
        explicit ShaderCollection(std::string name_, std::string source_, uint32_t hash_)
            : name(std::move(name_))
            , source(std::move(source_))
            , hash(hash_) {}
        ~ShaderCollection() = default;

        void BuildAndCacheShader(const std::string &entry, rhi::ShaderStageFlagBit stage, const ShaderCompileOption &option, ShaderBuildResult &result);

        const std::string &RequestSource() const { return source; }
        const std::string &GetName() const { return name; }
        uint32_t GetHash() const { return hash; }
    private:
        std::string name;
        std::string source;
        uint32_t hash;
    };
    using ShaderCollectionPtr = std::shared_ptr<ShaderCollection>;

} // namespace sky
