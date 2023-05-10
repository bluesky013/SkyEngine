//
// Created by Zach Lee on 2023/2/1.
//

#include <gles/FrameBuffer.h>

#include <set>

#include <core/logger/Logger.h>

#include <gles/Core.h>
#include <gles/Device.h>

namespace sky::gles {

    FrameBuffer::~FrameBuffer()
    {
//        for (auto &fb : fboList) {
//            if (fb.fbo != 0) {
//                glDeleteFramebuffers(1, &fb.fbo);
//            }
//        }
        auto deleteFB = [](Queue &queue, GLuint fbo) {
            queue.CreateTask([fbo]() {
                CHECK(glDeleteFramebuffers(1, &fbo));
            });
        };
        for (uint32_t i = 0; i < objects.size(); ++i) {
            auto &fbo = objects[i];
            if (fbo != 0) {
                deleteFB(*device.GetQueue(static_cast<rhi::QueueType>(i)), fbo);
            }
        }
    }

    bool FrameBuffer::Init(const Descriptor &desc)
    {
        extent = desc.extent;
        renderPass = std::static_pointer_cast<RenderPass>(desc.pass);
        objects.resize(device.getQueueNumber(), 0);

        attachments.reserve(desc.views.size());
        for (auto &attachment : desc.views) {
            attachments.emplace_back(std::static_pointer_cast<ImageView>(attachment));
        }
        for (auto &attachment : attachments) {
            surface = attachment->GetImage()->GetSurface();
            if (surface) {
                break;
            }
        }
        return true;
    }

    GLuint FrameBuffer::AcquireNativeHandle(uint32_t queueIndex)
    {
        auto &fbo = objects[queueIndex];
        if (fbo == 0) {
            CHECK(glGenFramebuffers(1, &fbo));
            InitInternal(fbo);
        }
        return fbo;
    }

    void FrameBuffer::InitInternal(GLuint fbo) {
        CHECK(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo));

        auto &colors = renderPass->GetGLColors();
        auto &resolves = renderPass->GetGLResolves();
        auto depthStencil = renderPass->GetGLDepthStencil();

        for (uint32_t i = 0; i < colors.size(); ++i) {
            const auto &color = colors[i];

            auto &attachment = attachments[color];
            const auto &image = attachment->GetImage();
            const auto &imageDesc = image->GetDescriptor();
            const auto &viewDesc = attachment->GetViewDesc();
            auto handle = image->GetNativeHandle();
            GLint baseLevel = static_cast<GLint>(viewDesc.subRange.baseLevel);

            if (imageDesc.samples != rhi::SampleCount::X1) {
//                CHECK(glFramebufferTexture2DMultisampleEXT(GL_DRAW_FRAMEBUFFER,
//                                                           static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + i), GL_TEXTURE_2D,
//                                                           image->GetNativeHandle(),
//                                                           viewDesc.subRange.baseLevel,
//                                                           static_cast<GLsizei>(imageDesc.samples)));
//
//                const auto &resolve = resolves[i];
//                attachment = attachments[resolve];
//                handle = attachment->GetImage()->GetNativeHandle();
//                baseLevel = attachment->GetViewDesc().subRange.baseLevel;
            }


            if (image->IsRenderBuffer()) {
                CHECK(glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + i),
                                                GL_RENDERBUFFER, handle));
            } else {
                CHECK(glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + i),
                                             GL_TEXTURE_2D, handle, baseLevel));
            }
        }

        if (depthStencil != INVALID_INDEX) {
            auto &attachment = attachments[depthStencil];
            auto &image = attachment->GetImage();
            auto &viewDesc = attachment->GetViewDesc();
            auto handle = image->GetNativeHandle();
            GLenum att =
                viewDesc.mask & rhi::AspectFlagBit::STENCIL_BIT ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT;
            if (image->IsRenderBuffer()) {
                CHECK(glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, att, GL_RENDERBUFFER, handle));
            } else {
                CHECK(glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, att, GL_TEXTURE_2D, handle,
                                             attachment->GetViewDesc().subRange.baseLevel));
            }
        }
        CHECK(glBindFramebuffer(GL_FRAMEBUFFER, 0));
    }

}
