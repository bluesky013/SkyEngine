//
// Created by Zach Lee on 2023/2/1.
//

#include <gles/FrameBuffer.h>
#include <gles/Core.h>
#include <gles/Device.h>

namespace sky::gles {

    FrameBuffer::~FrameBuffer()
    {
        for (auto &fb : fboList) {
            if (fb.fbo != 0) {
                glDeleteFramebuffers(1, &fb.fbo);
            }
        }
    }

    bool FrameBuffer::Init(const Descriptor &desc)
    {
        extent = desc.extent;
        renderPass = std::static_pointer_cast<RenderPass>(desc.pass);
        attachments.reserve(desc.views.size());
        for (auto &attachment : desc.views) {
            attachments.emplace_back(std::static_pointer_cast<ImageView>(attachment));
        }

        auto &subPasses = renderPass->GetSubPasses();
        fboList.reserve(subPasses.size());
        for (auto &sub : subPasses) {
            SurfacePtr surface;
            auto iter = std::find_if(sub.colors.begin(), sub.colors.end(), [&](uint32_t id) {
                surface = attachments[id]->GetImage()->GetSurface();
                return !!surface;
            });
            if (iter != sub.colors.end()) {
                // framebuffer 0
                fboList.emplace_back(FBOWithSurface{0, surface});
                continue;
            }


            GLuint fbo = 0;
            glGenFramebuffers(1, &fbo);
            CHECK(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo));

            uint32_t index = 0;
            for (auto &color : sub.colors) {
                auto &attachment = attachments[color];
                auto &image = attachment->GetImage();
                auto handle = image->GetNativeHandle();
                if (image->IsRenderBuffer()) {
                    CHECK(glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + index), GL_RENDERBUFFER, handle));
                } else {
                    CHECK(glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + index), GL_TEXTURE_2D, handle, attachment->GetViewDesc().subRange.baseLevel));
                }
                index++;
            }

            index = 0;
            for (auto &resolve : sub.resolves) {
                auto &attachment = attachments[resolve];
                auto &image = attachment->GetImage();
                auto &imageDesc = image->GetDescriptor();
                auto &viewDesc = attachment->GetViewDesc();

                CHECK(glFramebufferTexture2DMultisampleEXT(GL_DRAW_FRAMEBUFFER, static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + index), GL_TEXTURE_2D,
                                                           image->GetNativeHandle(),
                                                           viewDesc.subRange.baseLevel,
                                                           imageDesc.samples));
                index++;
            }

            if (sub.depthStencil != ~(0U)) {
                auto &attachment = attachments[sub.depthStencil];
                auto &image = attachment->GetImage();
                auto &viewDesc = attachment->GetViewDesc();
                auto handle = image->GetNativeHandle();
                GLenum att = viewDesc.mask & rhi::AspectFlagBit::STENCIL_BIT ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT;
                if (image->IsRenderBuffer()) {
                    CHECK(glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, att, GL_RENDERBUFFER, handle));
                } else {
                    CHECK(glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, att, GL_TEXTURE_2D, handle, attachment->GetViewDesc().subRange.baseLevel));
                }
            }
            fboList.emplace_back(FBOWithSurface{fbo, {}});
            CHECK(glBindFramebuffer(GL_FRAMEBUFFER, 0));
        }
        return true;
    }

}
