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

#include <render/RenderResource.h>
#include <render/resource/ResourceGroup.h>
#include <shader/ShaderCompiler.h>

namespace sky {
    class Program : public RenderResource {
    public:
        Program() = default;
        ~Program() override = default;

        RDResourceLayoutPtr RequestLayout(uint32_t index) const;
        const rhi::PipelineLayoutPtr &GetPipelineLayout() const { return pipelineLayout; }
        const std::vector<rhi::ShaderPtr> &GetShaders() const { return shaders; }

        void AddShader(const rhi::ShaderPtr &shader) { shaders.emplace_back(shader); }
        void MergeReflection(ShaderReflection &&refl);
        void BuildPipelineLayout();

    private:
        rhi::PipelineLayoutPtr pipelineLayout;
        std::vector<rhi::ShaderPtr> shaders;
        std::unique_ptr<ShaderReflection> reflection;
    };
    using RDProgramPtr = CounterPtr<Program>;

    class ShaderCollection : public RenderResource {
    public:
        explicit ShaderCollection(const std::string &name_, std::string source_, uint32_t hash_)
            : RenderResource(name_)
            , source(std::move(source_))
            , hash(hash_) {}
        ~ShaderCollection() override = default;

        void BuildAndCacheShader(const std::string &entry, rhi::ShaderStageFlagBit stage, const ShaderCompileOption &option, ShaderBuildResult &result);

        const std::string &RequestSource() const { return source; }
        uint32_t GetHash() const { return hash; }
    private:
        std::string source;
        uint32_t hash;
    };
    using ShaderCollectionPtr = CounterPtr<ShaderCollection>;
} // namespace sky
