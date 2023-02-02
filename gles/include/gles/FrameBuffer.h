//
// Created by Zach Lee on 2023/2/1.
//

#pragma once

#include <rhi/FrameBuffer.h>
#include <gles/DevObject.h>
#include <gles/ImageView.h>
#include <gles/RenderPass.h>

namespace sky::gles {

    struct FBOWithSurface {
        GLuint fbo;
        SurfacePtr surface;
    };

    class FrameBuffer : public rhi::FrameBuffer, public DevObject {
    public:
        FrameBuffer(Device &dev) : DevObject(dev) {}
        ~FrameBuffer();

        bool Init(const Descriptor &desc);
        const std::vector<FBOWithSurface> &GetNativeHandles() const { return fboList; }

    private:
        RenderPassPtr renderPass;
        std::vector<ImageViewPtr> attachments;
        std::vector<FBOWithSurface> fboList;
    };

}
