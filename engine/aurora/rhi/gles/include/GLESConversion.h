//
// Created on 2026/04/01.
//

#pragma once

#include <aurora/rhi/Core.h>
#include <GLESForward.h>

namespace sky::aurora {

    struct GLInternalFormat {
        GLenum internalFormat = 0;
        GLenum format         = 0;
        GLenum type           = 0;
        bool   compressed     = false;
    };

    struct GLVertexFormat {
        GLint     size       = 0;
        GLenum    type       = 0;
        GLboolean normalized = GL_FALSE;
    };

    // pixel format
    const GLInternalFormat &FromPixelFormat(PixelFormat format);
    const GLVertexFormat   &FromFormat(Format format);

    // sampler
    GLenum FromFilter(Filter filter);
    GLenum FromFilterMip(Filter filter, MipFilter mip);
    GLenum FromWrapMode(WrapMode mode);

    // pipeline state
    GLenum FromBlendFactor(BlendFactor factor);
    GLenum FromBlendOp(BlendOp op);
    GLenum FromCompareOp(CompareOp op);
    GLenum FromStencilOp(StencilOp op);
    GLenum FromPrimitiveTopology(PrimitiveTopology topo);
    GLenum FromFrontFace(FrontFace face);
    GLenum FromCullMode(const CullingModeFlags &flags);
    GLenum FromIndexType(IndexType type);

} // namespace sky::aurora
