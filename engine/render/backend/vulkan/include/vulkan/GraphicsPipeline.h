//
// Created by Zach Lee on 2022/1/9.
//

#pragma once

#include "rhi/GraphicsPipeline.h"
#include "vulkan/DevObject.h"
#include "vulkan/PipelineLayout.h"
#include "vulkan/RenderPass.h"
#include "vulkan/Shader.h"
#include "vulkan/ShaderConstants.h"
#include "vulkan/VertexInput.h"
#include "vulkan/vulkan.h"
#include <string>
#include <vector>
#include <array>

namespace sky::vk {

    class Device;

    class GraphicsPipeline : public rhi::GraphicsPipeline, public DevObject {
    public:
        ~GraphicsPipeline() override = default;

        VkPipeline GetNativeHandle() const;
        PipelineLayoutPtr GetPipelineLayout() const;

    private:
        friend class Device;
        explicit GraphicsPipeline(Device &);
        bool Init(const Descriptor &);
        static uint32_t CalculateHash(const Descriptor &);

        VkPipeline        pipeline;
        uint32_t          hash;
        RenderPassPtr     renderPass;
        PipelineLayoutPtr pipelineLayout;
        VertexInputPtr    inputDesc;
    };

    using GraphicsPipelinePtr = std::shared_ptr<GraphicsPipeline>;

} // namespace sky::vk
