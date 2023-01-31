//
// Created by Zach Lee on 2023/2/1.
//

#pragma once

#include <rhi/FrameBuffer.h>
#include <gles/DevObject.h>

namespace sky::gles {

    class FrameBuffer : public rhi::FrameBuffer, public DevObject {
    public:
        FrameBuffer(Device &dev) : DevObject(dev) {}
        ~FrameBuffer();

        bool Init(const Descriptor &desc);

    private:
        GLuint fbo = 0;
    };

}