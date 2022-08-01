//
// Created by Zach Lee on 2022/6/26.
//

#pragma once

#include <vulkan/vulkan.h>
#include <vulkan/DescriptorSet.h>
#include <vulkan/GraphicsPipeline.h>
#include <vulkan/VertexAssembly.h>
#include <vulkan/DescriptorSetBinder.h>

namespace sky::drv {

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

    enum CmdDrawType : uint8_t {
        LINEAR,
        INDEXED
    };

    struct CmdDraw {
        union {
            CmdDrawIndexed indexed;
            CmdDrawLinear linear;
        };
        CmdDrawType type;
    };

    inline CmdDraw MakeCmdDraw(const CmdDrawLinear& value)
    {
        CmdDraw draw = {};
        draw.linear = value;
        draw.type = CmdDrawType::LINEAR;
        return draw;
    }

    inline CmdDraw MakeCmdDraw(const CmdDrawIndexed& value)
    {
        CmdDraw draw = {};
        draw.indexed = value;
        draw.type = CmdDrawType::LINEAR;
        return draw;
    }

    struct CmdStencil {
        uint8_t reference;
    };

    struct DrawItem {
        uint8_t                viewportCount  = 0;
        uint8_t                scissorCount   = 0;
        CmdDraw*               drawArgs       = nullptr;
        VkViewport*            viewport       = nullptr;
        VkRect2D*              scissor        = nullptr;
        CmdStencil*            stencil        = nullptr;
        uint8_t*               pushConstants  = nullptr;
        GraphicsPipelinePtr    pso;
        DescriptorSetBinderPtr shaderResources;
        VertexAssemblyPtr      vertexAssembly;
    };

}