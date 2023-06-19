//
// Created by Zach Lee on 2023/2/1.
//

#include <gles/FrameBuffer.h>

#include <set>

#include <core/logger/Logger.h>

#include <gles/Core.h>
#include <gles/Device.h>
#include <gles/Ext.h>

namespace sky::gles {
    namespace {
        const char* TAG = "GLES";
    }

    FrameBuffer::~FrameBuffer()
    {
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
        objects.resize(device.GetQueueNumber(), 0);

        attachments.reserve(desc.views.size());
        for (auto &attachment : desc.views) {
            attachments.emplace_back(std::static_pointer_cast<ImageView>(attachment));
        }
        for (auto &attachment : attachments) {
            if (surface = attachment->GetImage()->GetSurface(); surface) {
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
        CHECK(glBindFramebuffer(GL_FRAMEBUFFER, fbo));

        auto &colors = renderPass->GetGLColors();
        auto &resolves = renderPass->GetGLResolves();
        auto depthStencil = renderPass->GetGLDepthStencil();
        auto dsResolve = renderPass->GetGLDSResolve();

        bool supportResolve = device.GetInternalFeature().msaa1;
        bool supportResolveMRT = device.GetInternalFeature().msaa2;

        blitPairs.clear();
        std::vector<GLenum> drawBuffers(colors.size());
        for (uint32_t i = 0; i < colors.size(); ++i) {
            const auto &color = colors[i];

            auto &attachment = attachments[color];
            const auto &image = attachment->GetImage();
            const auto &imageDesc = image->GetDescriptor();
            const auto &viewDesc = attachment->GetViewDesc();
            const auto &attachmentDesc = renderPass->GetAttachments()[color];
            auto handle = image->GetNativeHandle();
            GLint baseLevel = static_cast<GLint>(viewDesc.subRange.baseLevel);
            drawBuffers[i] = GL_COLOR_ATTACHMENT0 + i;

            if (imageDesc.samples != rhi::SampleCount::X1 && resolves[i] != INVALID_INDEX) {
                if (supportResolve && (i == 0 || supportResolveMRT)) {
                    const auto &resolve = resolves[i];
                    const auto &resolveAttachment = attachments[resolve];
                    auto resolveHandle = resolveAttachment->GetImage()->GetNativeHandle();
                    auto resolveBaseLevel = resolveAttachment->GetViewDesc().subRange.baseLevel;

                    CHECK(FramebufferTexture2DMultisampleEXT(GL_FRAMEBUFFER,
                                                             static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + i),
                                                             GL_TEXTURE_2D,
                                                             resolveHandle,
                                                             resolveBaseLevel,
                                                             static_cast<GLsizei>(imageDesc.samples)));
                    continue;
                } else {
                    blitPairs.emplace_back(std::pair<uint32_t, uint32_t>{colors[i], resolves[i]});
                }
            }

            if (image->IsRenderBuffer()) {
                CHECK(glFramebufferRenderbuffer(GL_FRAMEBUFFER, static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + i),
                                                GL_RENDERBUFFER, handle));
            } else {
                CHECK(glFramebufferTexture2D(GL_FRAMEBUFFER, static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + i),
                                             GL_TEXTURE_2D, handle, baseLevel));
            }
        }

        if (dsResolve != INVALID_INDEX) {
            if (supportResolve && supportResolveMRT) {
                const auto &resolveAttachment = attachments[dsResolve];
                auto msImageDesc = attachments[depthStencil]->GetImage()->GetDescriptor();
                auto resolveHandle = resolveAttachment->GetImage()->GetNativeHandle();
                auto resolveBaseLevel = resolveAttachment->GetViewDesc().subRange.baseLevel;

                CHECK(FramebufferTexture2DMultisampleEXT(GL_FRAMEBUFFER,
                                                         GL_DEPTH_STENCIL_ATTACHMENT ,
                                                         GL_TEXTURE_2D,
                                                         resolveHandle,
                                                         resolveBaseLevel,
                                                         static_cast<GLsizei>(msImageDesc.samples)));
                depthStencil = INVALID_INDEX;
            } else {
                blitPairs.emplace_back(std::pair<uint32_t, uint32_t>{depthStencil, dsResolve});
            }
        }

        if (depthStencil != INVALID_INDEX) {
            auto &attachment = attachments[depthStencil];
            auto &image = attachment->GetImage();
            auto &viewDesc = attachment->GetViewDesc();
            auto handle = image->GetNativeHandle();
            GLenum att = viewDesc.subRange.aspectMask & rhi::AspectFlagBit::STENCIL_BIT ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT;
            if (image->IsRenderBuffer()) {
                CHECK(glFramebufferRenderbuffer(GL_FRAMEBUFFER, att, GL_RENDERBUFFER, handle));
            } else {
                CHECK(glFramebufferTexture2D(GL_FRAMEBUFFER, att, GL_TEXTURE_2D, handle, viewDesc.subRange.baseLevel));
            }
        }
        CHECK(glDrawBuffers(static_cast<uint32_t>(drawBuffers.size()), drawBuffers.data()));

        GLenum status;
        CHECK(status = glCheckFramebufferStatus(GL_FRAMEBUFFER));
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            LOG_E(TAG, "check frame buffer status error - %x", status);
        }
        CHECK(glBindFramebuffer(GL_FRAMEBUFFER, 0));
    }

}
