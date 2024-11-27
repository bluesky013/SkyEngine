//
// Created by Zach Lee on 2023/8/31.
//

#pragma once

#include <vector>
#include <core/name/Name.h>
#include <render/resource/Shader.h>
#include <render/RenderResource.h>
#include <render/RenderBase.h>

namespace sky {

    struct ShaderRef {
        ShaderCollectionPtr shaderCollection;
        std::string objectOrCSMain;
        std::string vertOrMeshMain;
        std::string fragmentMain;
    };

    class Technique : public RenderResource {
    public:
        Technique() = default;
        ~Technique() override = default;

        void SetShader(const ShaderRef &shader);

        RDProgramPtr RequestProgram(const ShaderOptionPtr &option = nullptr);

    protected:
        virtual void FillProgramInternal(Program &program, const ShaderCompileOption &option) {}

        ShaderRef shaderData;

        std::unordered_map<uint32_t, RDProgramPtr> programCache;
    };

    class GraphicsTechnique : public Technique {
    public:
        GraphicsTechnique() = default;
        ~GraphicsTechnique() override = default;

        void SetDepthStencil(const rhi::DepthStencil &ds);
        void SetRasterState(const rhi::RasterState &rs);
        void SetBlendState(const std::vector<rhi::BlendState> &blends);
        void SetRasterTag(const Name &tag);
        void SetVertexFlag(RenderVertexFlagBit flagBit, const std::string &macro);

        void Process(RenderVertexFlags flags, const ShaderOptionPtr &option);

        const Name &GetRasterID() const { return rasterID; }
        uint32_t GetViewMask() const { return viewMask; }
        const rhi::PipelineState &GetPipelineState() const { return state; }

        static rhi::GraphicsPipelinePtr BuildPso(const RDProgramPtr &program,
                                                 const rhi::PipelineState &state,
                                                 const rhi::VertexInputPtr &vertexDesc,
                                                 const rhi::RenderPassPtr &pass,
                                                 uint32_t subPassID);

    private:
        void FillProgramInternal(Program &program, const ShaderCompileOption &option) override;
        rhi::PipelineState state;
        Name rasterID;
        uint32_t viewMask = 0xFFFFFFFF;

        std::vector<std::string> preCompiledFlags;
        std::unordered_map<RenderVertexFlagBit, std::string> vertexFlags;
    };
    using RDGfxTechPtr = CounterPtr<GraphicsTechnique>;

    class ComputeTechnique : public Technique {
    public:
        ComputeTechnique() = default;
        ~ComputeTechnique() override = default;

    private:
        void FillProgramInternal(Program &program, const ShaderCompileOption &option) override;

        rhi::PipelineState state;
    };
    using RDCompTechPtr = CounterPtr<ComputeTechnique>;
} // namespace sky
