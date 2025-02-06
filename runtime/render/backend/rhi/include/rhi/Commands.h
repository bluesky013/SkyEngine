//
// Created by Zach on 2023/1/31.
//

#pragma once

#include <rhi/Core.h>
#include <rhi/FrameBuffer.h>
#include <rhi/RenderPass.h>
#include <rhi/GraphicsPipeline.h>
#include <rhi/VertexAssembly.h>
#include <rhi/Buffer.h>
#include <rhi/Image.h>
#include <rhi/Semaphore.h>
#include <rhi/DescriptorSet.h>

namespace sky::rhi {

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

    struct CmdDispatchMesh {
        uint32_t x = 1;
        uint32_t y = 1;
        uint32_t z = 1;
    };

    enum CmdDrawType : uint8_t { LINEAR, INDEXED, MESH_DISPATCH };

    struct CmdDraw {
        union {
            CmdDrawIndexed  indexed;
            CmdDrawLinear   linear;
            CmdDispatchMesh dispatchMesh;
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

    inline CmdDraw MakeCmdDraw(const CmdDispatchMesh &mesh)
    {
        CmdDraw draw = {};
        draw.dispatchMesh = mesh;
        draw.type         = CmdDrawType::MESH_DISPATCH;
        return draw;
    }

    struct CmdStencil {
        uint8_t reference;
    };
}
