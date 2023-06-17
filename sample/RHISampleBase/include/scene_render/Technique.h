//
// Created by Zach Lee on 2023/4/7.
//

#pragma once

#include <memory>
#include <core/math/Matrix4.h>
#include <rhi/GraphicsPipeline.h>
#include <rhi/PipelineLayout.h>

namespace sky::rhi {

    struct GraphicsTechnique {
    public:
        GraphicsTechnique() = default;
        ~GraphicsTechnique() = default;

        void SetPipelineLayout(const PipelineLayoutPtr &layout) { psoDesc.pipelineLayout = layout; }
        void SetRenderPass(const RenderPassPtr &pass, uint32_t subPass = 0) { psoDesc.renderPass = pass; psoDesc.subPassIndex = subPass; }
        void SetVertexInput(const VertexInputPtr &vi) { psoDesc.vertexInput = vi; }
        void SetShader(const ShaderPtr &v, const ShaderPtr &f) { psoDesc.vs = v; psoDesc.fs = f; }
        void BuildPso();

        GraphicsPipeline::Descriptor psoDesc;
        GraphicsPipelinePtr pso;
    };
    using GFXTechniquePtr = std::shared_ptr<GraphicsTechnique>;

}