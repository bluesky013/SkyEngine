//
// Created by Zach Lee on 2022/6/26.
//

#pragma once

#include <vulkan/DescriptorSet.h>
#include <vulkan/DescriptorSetBinder.h>
#include <vulkan/GraphicsPipeline.h>
#include <vulkan/PushConstants.h>
#include <vulkan/VertexAssembly.h>
#include <vulkan/vulkan.h>

namespace sky::vk {

    struct CmdDrawIndexed {
        uint32_t indexCount    = 0;
        uint32_t instanceCount = 1;
        uint32_t firstIndex    = 0;
        int32_t  vertexOffset  = 0;
        uint32_t firstInstance = 0;
    };

    struct CmdDrawLinear {
        uint32_t vertexCount   = 0;
        uint32_t instanceCount = 1;
        uint32_t firstVertex   = 0;
        uint32_t firstInstance = 0;
    };

    enum CmdDrawType : uint8_t { LINEAR, INDEXED };

    struct CmdDraw {
        union {
            CmdDrawIndexed indexed;
            CmdDrawLinear  linear;
        };
        CmdDrawType type;
    };

    inline CmdDraw MakeCmdDraw(const CmdDrawLinear &value)
    {
        CmdDraw draw = {};
        draw.linear  = value;
        draw.type    = CmdDrawType::LINEAR;
        return draw;
    }

    inline CmdDraw MakeCmdDraw(const CmdDrawIndexed &value)
    {
        CmdDraw draw = {};
        draw.indexed = value;
        draw.type    = CmdDrawType::LINEAR;
        return draw;
    }

    struct CmdStencil {
        uint8_t reference;
    };

    struct DrawItem {
        uint8_t                viewportCount = 0;
        uint8_t                scissorCount  = 0;
        CmdDraw                drawArgs      = {};
        VkViewport            *viewport      = nullptr;
        VkRect2D              *scissor       = nullptr;
        CmdStencil            *stencil       = nullptr;
        PushConstantsPtr       pushConstants = nullptr;
        GraphicsPipelinePtr    pso;
        DescriptorSetBinderPtr shaderResources;
        VertexAssemblyPtr      vertexAssembly;
    };

} // namespace sky::vk
