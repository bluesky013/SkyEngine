//
// Created by Zach Lee on 2023/2/1.
//

#pragma once

#include <rhi/FrameBuffer.h>
#include <gles/DevObject.h>
#include <gles/ImageView.h>
#include <gles/RenderPass.h>

namespace sky::gles {

    class FrameBuffer : public rhi::FrameBuffer, public DevObject {
    public:
        FrameBuffer(Device &dev) : DevObject(dev) {}
        ~FrameBuffer();

        bool Init(const Descriptor &desc);
        GLuint AcquireNativeHandle(uint32_t queueIndex);
        const SurfacePtr &GetSurface() const { return surface; }

    private:
        void InitInternal(GLuint fbo);

        RenderPassPtr renderPass;
        std::vector<ImageViewPtr> attachments;

        SurfacePtr surface;
        // OpenGL-ES explicitly disallows sharing of fbo objects
        std::vector<GLuint> objects;
    };
    using FrameBufferPtr = std::shared_ptr<FrameBuffer>;

}
