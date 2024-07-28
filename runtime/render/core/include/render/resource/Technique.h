//
// Created by Zach Lee on 2023/8/31.
//

#pragma once

#include <vector>
#include <render/resource/Shader.h>
#include <render/RenderResource.h>

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

        RDProgramPtr RequestProgram(const ShaderPreprocessorPtr &preprocessor = nullptr);

    protected:
        virtual void FillProgramInternal(Program &program, const ShaderCompileOption &option) {}

        ShaderRef shaderData;
    };

    class GraphicsTechnique : public Technique {
    public:
        GraphicsTechnique() = default;
        ~GraphicsTechnique() override = default;

        void SetDepthStencil(const rhi::DepthStencil &ds);
        void SetRasterState(const rhi::RasterState &rs);
        void SetBlendState(const std::vector<rhi::BlendState> &blends);
        void SetRasterTag(const std::string &tag);
        void SetVertexLayout(const std::string &key);

        uint32_t GetRasterID() const { return rasterID; }
        uint32_t GetViewMask() const { return viewMask; }
        const std::string &GetVertexDescKey() const { return vertexDescKey; }
        const rhi::PipelineState &GetPipelineState() const { return state; }

        static rhi::GraphicsPipelinePtr BuildPso(GraphicsTechnique &tech,
                                                 const rhi::RenderPassPtr &pass,
                                                 uint32_t subPassID);

    private:
        void FillProgramInternal(Program &program, const ShaderCompileOption &option) override;
        rhi::PipelineState state;
        std::string vertexDescKey;
        rhi::VertexInputPtr vertexDesc;
        uint32_t rasterID = 0;
        uint32_t viewMask = 0xFFFFFFFF;
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
