//
// Created by Zach Lee on 2022/11/13.
//

#pragma once

#include <rhi/Core.h>
#include <rhi/VertexInput.h>
#include <rhi/RenderPass.h>
#include <rhi/PipelineLayout.h>
#include <rhi/Shader.h>

namespace sky::rhi {

    class GraphicsPipeline {
    public:
        GraphicsPipeline()          = default;
        virtual ~GraphicsPipeline() = default;

        struct Descriptor {
            PipelineState     state;
            ShaderPtr         vs;
            ShaderPtr         fs;
            VertexInputPtr    vertexInput;
            RenderPassPtr     renderPass;
            PipelineLayoutPtr pipelineLayout;
            uint32_t          subPassIndex = 0;
        };
    };

}