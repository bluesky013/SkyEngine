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
            ShaderPtr         tas;
            ShaderPtr         ms;
            VertexInputPtr    vertexInput;
            RenderPassPtr     renderPass;
            PipelineLayoutPtr pipelineLayout;
            uint32_t          subPassIndex = 0;
        };

        uint32_t GetDescriptorMask() const { return descriptorMask; }

    protected:
        uint32_t descriptorMask = 0;
    };
    using GraphicsPipelinePtr = std::shared_ptr<GraphicsPipeline>;
}
