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

        void SetShaderTable(RDGfxShaderTablePtr);

        void SetRenderPass(RDPassPtr pass);

        void InitRHI() override;

        bool IsValid() const override;

    private:
        RDGfxShaderTablePtr table;
        RDPassPtr pass;
        drv::GraphicsPipeline::State pipelineState;
        drv::GraphicsPipeline::Program program;
        drv::GraphicsPipelinePtr pso;
    };
    using RDGfxTechniquePtr = std::shared_ptr<GraphicsTechnique>;

}