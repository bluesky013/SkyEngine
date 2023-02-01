//
// Created by Zach Lee on 2023/2/1.
//

#include <gles/FrameBuffer.h>
#include <rhi/Device.h>

namespace sky::gles {

    bool FrameBuffer::Init(const Descriptor &desc)
    {
        descriptor = desc;
        
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);


        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return true;
    }

}