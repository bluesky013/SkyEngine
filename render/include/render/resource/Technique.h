//
// Created by Zach Lee on 2023/8/31.
//

#pragma once

#include <vector>
#include <render/resource/Shader.h>

namespace sky {

    class Technique {
    public:
        Technique() = default;
        virtual ~Technique() = default;

        void AddShader(const RDShaderPtr &shader);
        RDProgramPtr RequestProgram(const std::string &key = "");

    protected:
        std::vector<RDShaderPtr> shaders;
        std::unordered_map<std::string, RDProgramPtr> programs;
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
        const rhi::PipelineState &GetPipelineState() const { return state; }

        static rhi::GraphicsPipelinePtr BuildPso(GraphicsTechnique &tech,
                                                 const rhi::RenderPassPtr &pass,
                                                 uint32_t subPassID,
                                                 const std::string &programKey = "");

    private:
        rhi::PipelineState state;
        rhi::VertexInputPtr vertexDesc;
        uint32_t rasterID = 0;
        uint32_t viewMask = 0xFFFFFFFF;
    };
    using RDGfxTechPtr = std::shared_ptr<GraphicsTechnique>;

    class ComputeTechnique : public Technique {
    public:
        ComputeTechnique() = default;
        ~ComputeTechnique() override = default;
    };
    using RDCompTechPtr = std::shared_ptr<ComputeTechnique>;
} // namespace sky
