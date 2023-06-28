#pragma once

#include <rhi/FrameBuffer.h>
#include <gles/ImageView.h>
#include <gles/RenderPass.h>
#include <gles/egl/Surface.h>
#include <vector>

namespace sky::gles {

    struct FramebufferObject : public DevObject {
        FramebufferObject(Device &dev);
        ~FramebufferObject();

        using Attachment = rhi::RenderPass::Attachment;

        GLuint AcquireNativeHandle(uint32_t queueIndex);
        void InitInternal();

        void BindColor(const ImageViewPtr &view, const Attachment &attachment, uint32_t colorIndex, rhi::SampleCount samples = rhi::SampleCount::X1);
        void BindDepthStencil(const ImageViewPtr &view, const Attachment &attachment, rhi::SampleCount samples = rhi::SampleCount::X1);

        std::vector<GLenum> preInvalidates;
        std::vector<GLenum> postInvalidates;

        std::vector<ImageViewPtr> colors;
        ImageViewPtr depthStencil;

        std::vector<rhi::SampleCount> colorResolves;
        rhi::SampleCount depthStencilResolve;

        SurfacePtr surface;
        // OpenGL-ES explicitly disallows sharing of fbo objects
        std::vector<GLuint> objects;
    };

} // namespace sky::gles
