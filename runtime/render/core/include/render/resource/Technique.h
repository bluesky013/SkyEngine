//
// Created by Zach Lee on 2023/8/31.
//

#pragma once

#include <core/name/Name.h>
#include <render/resource/Shader.h>
#include <render/RenderResource.h>
#include <render/RenderBase.h>
#include <shader/ShaderCompiler.h>
#include <shader/ShaderVariant.h>

#include <vector>

namespace sky {

    struct ShaderRef {
        Name shaderName;
        std::string objectOrCSMain;
        std::string vertOrMeshMain;
        std::string fragmentMain;
    };

    class Technique : public RenderResource {
    public:
        Technique() = default;
        ~Technique() override = default;

        void SetShader(const ShaderRef &shader);

        void SetOption(const Name& name, uint8_t val, ShaderVariantKey &variantKey);

        template <typename Func>
        void ForEachOption(Func &&fn)
        {
            if (shader) {
                shader->ForEachOption(std::forward<Func>(fn));
            }
        }

        RDProgramPtr RequestProgram(const ShaderVariantKey &key = {});

    protected:
        virtual RDProgramPtr FillProgramInternal(const ShaderVariantKey &key) = 0;

        ShaderRef shaderData;
        ShaderCollectionPtr shader;
    };

    class GraphicsTechnique : public Technique {
    public:
        GraphicsTechnique() = default;
        ~GraphicsTechnique() override = default;

        void SetDepthStencil(const rhi::DepthStencil &ds);
        void SetRasterState(const rhi::RasterState &rs);
        void SetBlendState(const std::vector<rhi::BlendState> &blends);
        void SetRasterTag(const Name &tag);
        void AddVertexFlag(RenderVertexFlagBit flagBit, const Name &key);
        void ProcessVertexVariantKey(RenderVertexFlags flags, ShaderVariantKey &key);

        const Name &GetRasterID() const { return rasterID; }
        uint32_t GetViewMask() const { return viewMask; }
        const rhi::PipelineState &GetPipelineState() const { return state; }

        static rhi::GraphicsPipelinePtr BuildPso(const RDProgramPtr &program,
                                                 const rhi::PipelineState &state,
                                                 const rhi::VertexInputPtr &vertexDesc,
                                                 const rhi::RenderPassPtr &pass,
                                                 uint32_t subPassID);

    private:
        RDProgramPtr FillProgramInternal(const ShaderVariantKey &key) override;
        rhi::PipelineState state;
        Name rasterID;
        uint32_t viewMask = 0xFFFFFFFF;
        std::unordered_map<RenderVertexFlagBit, Name> vertexFlags;
    };
    using RDGfxTechPtr = CounterPtr<GraphicsTechnique>;

    class ComputeTechnique : public Technique {
    public:
        ComputeTechnique() = default;
        ~ComputeTechnique() override = default;

    private:
        RDProgramPtr FillProgramInternal(const ShaderVariantKey &key) override;

        rhi::PipelineState state;
    };
    using RDCompTechPtr = CounterPtr<ComputeTechnique>;
} // namespace sky
