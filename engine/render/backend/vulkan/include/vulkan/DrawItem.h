//
// Created by Zach Lee on 2022/6/26.
//

#pragma once

#include <rhi/Commands.h>
#include <vulkan/DescriptorSet.h>
#include <vulkan/DescriptorSetBinder.h>
#include <vulkan/GraphicsPipeline.h>
#include <vulkan/PushConstants.h>
#include <vulkan/VertexAssembly.h>
#include <vulkan/vulkan.h>

namespace sky::vk {

    struct DrawItem {
        uint8_t                viewportCount = 0;
        uint8_t                scissorCount  = 0;
        rhi::CmdDraw          drawArgs      = {};
        VkViewport            *viewport      = nullptr;
        VkRect2D              *scissor       = nullptr;
        rhi::CmdStencil       *stencil       = nullptr;
        PushConstantsPtr       pushConstants = nullptr;
        GraphicsPipelinePtr    pso;
        DescriptorSetBinderPtr shaderResources;
        VertexAssemblyPtr      vertexAssembly;
    };

} // namespace sky::vk
