//
// Created by Zach Lee on 2023/8/31.
//

#pragma once

#include <utility>
#include <vector>
#include <memory>
#include <rhi/Device.h>
#include <core/name/Name.h>
#include <render/RenderResource.h>
#include <render/resource/ResourceGroup.h>
#include <shader/ShaderCacheManager.h>

namespace sky {
    class Program : public RenderResource {
    public:
        Program() = default;
        ~Program() override = default;

        RDResourceLayoutPtr RequestLayout(uint32_t index) const;

        const rhi::PipelineLayoutPtr &GetPipelineLayout() const { return pipelineLayout; }
        const std::vector<rhi::ShaderPtr> &GetShaders() const { return shaders; }
        const std::vector<VertexStageAttribute> &GetVertexAttributes() const { return reflection->attributes; }
        const ShaderReflection* GetReflection() const { return reflection.get(); }

        void AddShader(const rhi::ShaderPtr &shader) { shaders.emplace_back(shader); }
        void MergeReflection(ShaderReflection &&refl);
        void Build();

    private:
        rhi::PipelineLayoutPtr pipelineLayout;
        std::vector<rhi::ShaderPtr> shaders;
        std::unique_ptr<ShaderReflection> reflection;
    };
    using RDProgramPtr = CounterPtr<Program>;

    class ShaderCollection : public RenderResource {
    public:
        explicit ShaderCollection(const Name &name_, const ShaderSourceEntry &source);
        ~ShaderCollection() override = default;

        RDProgramPtr FetchProgramCache(const ShaderVariantKey &key = {}) const;
        void CacheProgram(const ShaderVariantKey &key, const RDProgramPtr &program);

        RDProgramPtr AcquireShaderBinary(const ShaderVariantKey &key, const std::vector<std::pair<Name, rhi::ShaderStageFlagBit>> &stages);
        static bool BuildShaderBinary(const ShaderSourceDesc &source, const ShaderCompileOption &option, ShaderBuildResult &result);

        template <typename Func>
        void ForEachOption(Func &&fn)
        {
            list.ForeachOptions(std::forward<Func>(fn));
        }

        void SetOption(const Name& name_, uint8_t val, ShaderVariantKey &variantKey)
        {
            list.SetValue(name_, val, variantKey);
        }

    private:
        ShaderVariantList list;

        mutable std::mutex mutex;
        std::unordered_map<ShaderVariantKey, RDProgramPtr> programCache;
    };
    using ShaderCollectionPtr = CounterPtr<ShaderCollection>;
} // namespace sky
