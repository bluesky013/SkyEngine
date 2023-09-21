//
// Created by Zach Lee on 2023/6/19.
//

#include <gles/FrameBufferBlitHelper.h>

namespace sky::gles {

    FrameBufferBlitHelper &FrameBufferBlitHelper::SetSrcTarget(GLuint fbo, uint32_t colorIndex)
    {
        CHECK(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo));
        srcAttachment = colorIndex != INVALID_INDEX ? GL_COLOR_ATTACHMENT0 + colorIndex : GL_NONE;
        return *this;
    }

    FrameBufferBlitHelper &FrameBufferBlitHelper::SetSrcTarget(GLuint fbo, const ImageViewPtr &view)
    {
        CHECK(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo));
        BindTexture(view);
        srcAttachment = GL_COLOR_ATTACHMENT0;
        return *this;
    }

    FrameBufferBlitHelper &FrameBufferBlitHelper::SetDrawFrameBuffer(GLuint fbo)
    {
        CHECK(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo));
        return *this;
    }

    FrameBufferBlitHelper &FrameBufferBlitHelper::SetDstTarget(const ImageViewPtr &view)
    {
        BindTexture(view);
        return *this;
    }

    FrameBufferBlitHelper &FrameBufferBlitHelper::SetFilter(rhi::Filter filter)
    {
        blitFilter = FromRHI(filter);
        return *this;
    }

    void FrameBufferBlitHelper::Reset()
    {
        srcAttachment = GL_NONE;
        dstAttachment = GL_NONE;
        blitFilter = GL_NEAREST;
        mask = 0;
    }

    void FrameBufferBlitHelper::BindTexture(const ImageViewPtr &view)
    {
        auto &image = view->GetImage();
        auto &viewDesc = view->GetViewDesc();
        auto handle = image->GetNativeHandle();

        rhi::AspectFlags dsMask = rhi::AspectFlagBit::STENCIL_BIT | rhi::AspectFlagBit::STENCIL_BIT;
        if ((viewDesc.subRange.aspectMask & rhi::AspectFlagBit::COLOR_BIT)) {
            dstAttachment = GL_COLOR_ATTACHMENT0;
            mask |= GL_COLOR_BUFFER_BIT;
        } else if ((viewDesc.subRange.aspectMask & dsMask) == dsMask) {
            dstAttachment = GL_DEPTH_STENCIL_ATTACHMENT;
            mask |= (GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        } else if (viewDesc.subRange.aspectMask & rhi::AspectFlagBit::DEPTH_BIT) {
            dstAttachment = GL_DEPTH_ATTACHMENT;
            mask |= GL_DEPTH_BUFFER_BIT;
        } else if (viewDesc.subRange.aspectMask & rhi::AspectFlagBit::STENCIL_BIT) {
            dstAttachment = GL_STENCIL_ATTACHMENT;
            mask |= GL_STENCIL_BUFFER_BIT;
        }

        if (image->IsRenderBuffer()) {
            CHECK(glFramebufferRenderbuffer(GL_FRAMEBUFFER, dstAttachment, GL_RENDERBUFFER, handle));
        } else {
            CHECK(glFramebufferTexture2D(GL_FRAMEBUFFER, dstAttachment, GL_TEXTURE_2D, handle, viewDesc.subRange.baseLevel));
        }
    }

    void FrameBufferBlitHelper::Blit(const rhi::Extent2D &ext)
    {
        Blit(ext, ext);
    }

    void FrameBufferBlitHelper::Blit(const rhi::Extent2D &src, const rhi::Extent2D &dst)
    {
        if (mask & GL_COLOR_BUFFER_BIT) {
            CHECK(glReadBuffer(srcAttachment));
            CHECK(glDrawBuffers(1, &dstAttachment));
        }

        CHECK(glBlitFramebuffer(
            0, 0, src.width, src.height,
            0, 0, dst.width, dst.height,
            mask, blitFilter));
    }

} // namespace sky::gles
