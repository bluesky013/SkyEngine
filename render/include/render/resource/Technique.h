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
        RDProgramPtr RequestProgram(const std::string &key);

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
        uint32_t GetRasterID() const { return rasterID; }

        static rhi::GraphicsPipelinePtr BuildPso(GraphicsTechnique &tech,
                                                 const rhi::VertexInputPtr &vertexDesc,
                                                 const rhi::RenderPassPtr &pass,
                                                 uint32_t subPassID,
                                                 const std::string &programKey = "");

    private:
        rhi::PipelineState state;
        uint32_t rasterID;
    };
    using RDGfxTechPtr = std::shared_ptr<GraphicsTechnique>;

    class ComputeTechnique : public Technique {
    public:
        ComputeTechnique() = default;
        ~ComputeTechnique() override = default;
    };
    using RDCompTechPtr = std::shared_ptr<ComputeTechnique>;
} // namespace sky
