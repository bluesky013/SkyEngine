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
        void BuildMainProgram();

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

    private:
        rhi::PipelineState state;
        rhi::RenderPassPtr renderPass;
        uint32_t subPassID = 0;
    };
    using RDGfxTechPtr = std::shared_ptr<GraphicsTechnique>;

    class ComputeTechnique : public Technique {
    public:
        ComputeTechnique() = default;
        ~ComputeTechnique() override = default;
    };
    using RDCompTechPtr = std::shared_ptr<ComputeTechnique>;

} // namespace sky
