//
// Created by Zach Lee on 2022/5/28.
//

#pragma once

#include <render/resources/RenderResource.h>
#include <render/resources/Shader.h>
#include <render/resources/Pass.h>
#include <vulkan/GraphicsPipeline.h>

namespace sky {

    class Technique : public RenderResource {
    public:
        Technique() = default;
        ~Technique() = default;
    };
    using RDTechniquePtr = std::shared_ptr<Technique>;

    class GraphicsTechnique : public Technique {
    public:
        GraphicsTechnique() = default;
        ~GraphicsTechnique() = default;

        void SetShaderTable(RDGfxShaderTablePtr table);

        void SetRenderPass(RDPassPtr pass, uint32_t subPass = 0);

        drv::GraphicsPipelinePtr AcquirePso(drv::VertexInputPtr vertexInput);

        drv::GraphicsPipelinePtr AcquirePso(drv::VertexInputPtr vi, drv::ShaderOptionPtr option);

    private:
        RDGfxShaderTablePtr table;
        uint32_t subPassIndex = 0;
        RDPassPtr pass;
        drv::GraphicsPipeline::State pipelineState;
        std::unordered_map<uint32_t, drv::GraphicsPipelinePtr> psoCache;
    };
    using RDGfxTechniquePtr = std::shared_ptr<GraphicsTechnique>;

}