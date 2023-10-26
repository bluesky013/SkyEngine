//
// Created by Zach Lee on 2022/11/10.
//

#pragma once

#include <rhi/Core.h>
#include <gles/Core.h>

namespace sky::gles {

    struct InternalFormat {
        GLenum internal = 0;
        GLenum format   = 0;
        GLenum type     = 0;
    };

    struct FormatFeature {
        bool texture      = false;
        bool renderBuffer = false;
        bool compressed   = false;
    };

    struct VertexFormat {
        GLint     size = 0;
        GLenum    type = 0;
        GLboolean normalized = 0;
    };

    const InternalFormat &GetInternalFormat(rhi::PixelFormat format);
    const FormatFeature &GetFormatFeature(rhi::PixelFormat format);
    const VertexFormat &GetVertexFormat(rhi::Format format);

    GLenum FromRHI(rhi::Filter, rhi::MipFilter);
    GLenum FromRHI(rhi::Filter);
    GLenum FromRHI(rhi::WrapMode mode);
    GLenum FromRHI(rhi::CompareOp compare);
    GLenum FromRHI(rhi::CullingModeFlags flags);
    GLenum FromRHI(rhi::FrontFace frontFace);
    GLenum FromRHI(rhi::PrimitiveTopology topo);

    StencilState FromRHI(const rhi::StencilState &state);
    BlendTarget FromRHI(const rhi::BlendState &state);
}
