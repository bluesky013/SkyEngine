//
// Created by Zach Lee on 2023/2/1.
//

#pragma once

#include <rhi/FrameBuffer.h>
#include <gles/DevObject.h>
#include <gles/ImageView.h>
#include <gles/RenderPass.h>
#include <gles/FrameBufferObject.h>

namespace sky::gles {

    class FrameBuffer : public rhi::FrameBuffer, public DevObject {
    public:
        FrameBuffer(Device &dev) : DevObject(dev) {}
        ~FrameBuffer();

        bool Init(const Descriptor &fbDesc);
        FramebufferObject *GetFbo() const;

        bool NeedResolve() const;
        void DoResolve(uint32_t queueIndex) const;
    private:
        RenderPassPtr renderPass;
        std::vector<ImageViewPtr> attachments;
        std::vector<std::pair<uint32_t, uint32_t>> colorBlitPairs;
        GLbitfield dsResolveMask = 0;

        std::unique_ptr<FramebufferObject> framebuffer;
        std::unique_ptr<FramebufferObject> resolveFramebuffer;
    };
    using FrameBufferPtr = std::shared_ptr<FrameBuffer>;

}
