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
        std::pair<GLuint, GLuint> AcquireNativeHandle(uint32_t queueIndex);
        const ImageViewPtr &GetAttachment(uint32_t index) const { return attachments[index]; }
        uint32_t GetColorIndex(uint32_t attachmentIndex) const { return renderPass->GetAttachmentColorMap()[attachmentIndex]; }

        std::vector<std::pair<uint32_t, uint32_t>> GetBlitPairs() const { return blitPairs; }
        const SurfacePtr &GetSurface() const { return surface; }

    private:
        void InitInternal(GLuint fbo, GLuint &resolve);

        RenderPassPtr renderPass;
        std::vector<ImageViewPtr> attachments;
        std::vector<std::pair<uint32_t, uint32_t>> blitPairs;

        SurfacePtr surface;
        // OpenGL-ES explicitly disallows sharing of fbo objects
        std::vector<GLuint> objects;
        std::vector<GLuint> resolveObjects;
    };
    using FrameBufferPtr = std::shared_ptr<FrameBuffer>;

}
