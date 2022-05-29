//
// Created by Zach Lee on 2022/5/28.
//

#pragma once

#include <render/resources/RenderResource.h>
#include <render/resources/Shader.h>
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

    private:
        RDGfxShaderTablePtr table;
        drv::GraphicsPipeline::State pipelineState;
    };
    using RDGfxTechniquePtr = std::shared_ptr<GraphicsTechnique>;

}