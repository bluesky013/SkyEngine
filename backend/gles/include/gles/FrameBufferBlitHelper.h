//
// Created by Zach Lee on 2023/6/19.
//

#pragma once

#include <gles/Forward.h>
#include <gles/ImageView.h>

namespace sky::gles {

    struct FrameBufferBlitHelper {
        FrameBufferBlitHelper() = default;
        ~FrameBufferBlitHelper() = default;

        FrameBufferBlitHelper &SetSrcTarget(GLuint fbo, uint32_t colorIndex);
        FrameBufferBlitHelper &SetSrcTarget(GLuint fbo, const ImageViewPtr &view);

        FrameBufferBlitHelper &SetDrawFrameBuffer(GLuint fbo);
        FrameBufferBlitHelper &SetDstTarget(const ImageViewPtr &view);

        FrameBufferBlitHelper &SetFilter(rhi::Filter filter);

        void Blit(const rhi::Extent2D &ext);
        void Blit(const rhi::Extent2D &src, const rhi::Extent2D &dst);
        void BindTexture(const ImageViewPtr &view);

        void Reset();

        GLenum srcAttachment = GL_NONE;
        GLenum dstAttachment = GL_NONE;
        GLenum blitFilter = GL_NEAREST;
        GLbitfield mask = 0;
    };

} // namespace sky::gles