#include <gles/Device.h>
#include <gles/FrameBufferObject.h>
#include <gles/Queue.h>
#include <gles/Ext.h>

namespace sky::gles {
    namespace {
        const char *TAG = "GLES";
    }

    FramebufferObject::FramebufferObject(Device &dev) : DevObject(dev)
    {
        objects.resize(device.GetQueueNumber(), 0);
    }

    FramebufferObject::~FramebufferObject()
    {
        auto deleteFB = [](Queue &queue, GLuint fbo) { queue.CreateTask([fbo]() { CHECK(glDeleteFramebuffers(1, &fbo)); }); };
        for (uint32_t i = 0; i < objects.size(); ++i) {
            auto &fbo = objects[i];
            if (fbo != 0) {
                deleteFB(*device.GetQueue(static_cast<rhi::QueueType>(i)), fbo);
            }
        }
    }

    void FramebufferObject::BindColor(const ImageViewPtr &view, const Attachment &attachment, uint32_t colorIndex, rhi::SampleCount samples)
    {
        if (colorIndex >= colors.size()) {
            colors.resize(colorIndex + 1);
            colorResolves.resize(colorIndex + 1);
        }

        surface = view->GetImage()->GetSurface();
        bool isDefaultFb = !!surface;

        if (attachment.load == rhi::LoadOp::DONT_CARE) {
            preInvalidates.emplace_back(isDefaultFb ? GL_COLOR : GL_COLOR_ATTACHMENT0 + colorIndex);
        }
        if (attachment.store == rhi::StoreOp::DONT_CARE) {
            postInvalidates.emplace_back(isDefaultFb ? GL_COLOR : GL_COLOR_ATTACHMENT0 + colorIndex);
        }
        colors[colorIndex] = view;
        colorResolves[colorIndex] = samples;
    }

    void FramebufferObject::BindDepthStencil(const ImageViewPtr &view, const Attachment &attachment, rhi::SampleCount samples)
    {
        bool isDefaultFb = !!surface;

        bool hasDepth = HasDepth(attachment.format);
        bool hasStencil = HasStencil(attachment.format);
        if (hasDepth) {
            auto att = isDefaultFb ? GL_DEPTH : GL_DEPTH_ATTACHMENT;
            if (attachment.load == rhi::LoadOp::DONT_CARE) {
                preInvalidates.emplace_back(att);
            }
            if (attachment.store == rhi::StoreOp::DONT_CARE) {
                postInvalidates.emplace_back(att);
            }
        }
        if (hasStencil) {
            auto att = isDefaultFb ? GL_STENCIL : GL_STENCIL_ATTACHMENT;
            if (attachment.stencilLoad == rhi::LoadOp::DONT_CARE) {
                preInvalidates.emplace_back(att);
            }
            if (attachment.stencilStore == rhi::StoreOp::DONT_CARE) {
                postInvalidates.emplace_back(att);
            }
        }
        depthStencil = view;
        depthStencilResolve = samples;
    }

    GLuint FramebufferObject::AcquireNativeHandle(Context &context, uint32_t queueIndex)
    {
        if (surface) {
            context.MakeCurrent(*surface);
            return 0; // default framebuffer
        }

        auto &fbo = objects[queueIndex];
        if (fbo == 0) {
            CHECK(glGenFramebuffers(1, &fbo));
            CHECK(glBindFramebuffer(GL_FRAMEBUFFER, fbo));
            InitInternal();
        }
        return fbo;
    }

    void FramebufferObject::InitInternal()
    {
        std::vector<GLenum> drawBuffers(colors.size());
        for (uint32_t i = 0; i < colors.size(); ++i) {
            const auto &color = colors[i];
            const auto &resolveSample = colorResolves[i];

            const auto &image = color->GetImage();
            const auto &imageDesc = image->GetDescriptor();
            const auto &viewDesc = color->GetViewDesc();
            auto handle = image->GetNativeHandle();
            GLint baseLevel = static_cast<GLint>(viewDesc.subRange.baseLevel);
            drawBuffers[i] = GL_COLOR_ATTACHMENT0 + i;

            if (resolveSample != rhi::SampleCount::X1) {
                CHECK(FramebufferTexture2DMultisampleEXT(GL_FRAMEBUFFER,
                                                         static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + i),
                                                         GL_TEXTURE_2D,
                                                         handle,
                                                         baseLevel,
                                                         static_cast<GLsizei>(resolveSample)));
                continue;
            }

            if (image->IsRenderBuffer()) {
                CHECK(glFramebufferRenderbuffer(GL_FRAMEBUFFER, static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + i),
                                                GL_RENDERBUFFER, handle));
            } else {
                CHECK(glFramebufferTexture2D(GL_FRAMEBUFFER, static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + i),
                                             GL_TEXTURE_2D, handle, baseLevel));
            }
        }

        if (depthStencil) {
            auto &image = depthStencil->GetImage();
            auto &viewDesc = depthStencil->GetViewDesc();
            auto handle = image->GetNativeHandle();
            GLint baseLevel = static_cast<GLint>(viewDesc.subRange.baseLevel);
            auto mask = viewDesc.subRange.aspectMask;
            GLenum att = mask == (rhi::AspectFlagBit::DEPTH_BIT | rhi::AspectFlagBit::STENCIL_BIT) ? GL_DEPTH_STENCIL_ATTACHMENT :
                mask & rhi::AspectFlagBit::DEPTH_BIT ? GL_DEPTH_ATTACHMENT : GL_STENCIL_ATTACHMENT;

            if (depthStencilResolve != rhi::SampleCount::X1) {
                CHECK(FramebufferTexture2DMultisampleEXT(GL_FRAMEBUFFER,
                                                         att,
                                                         GL_TEXTURE_2D,
                                                         handle,
                                                         baseLevel,
                                                         static_cast<GLsizei>(depthStencilResolve)));
            } else {
                if (image->IsRenderBuffer()) {
                    CHECK(glFramebufferRenderbuffer(GL_FRAMEBUFFER, att, GL_RENDERBUFFER, handle));
                } else {
                    CHECK(glFramebufferTexture2D(GL_FRAMEBUFFER, att, GL_TEXTURE_2D, handle, viewDesc.subRange.baseLevel));
                }
            }
        }

        if (!drawBuffers.empty()) {
            CHECK(glDrawBuffers(static_cast<uint32_t>(drawBuffers.size()), drawBuffers.data()));
        }

        GLenum status;
        CHECK(status = glCheckFramebufferStatus(GL_FRAMEBUFFER));
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            SKY_ASSERT(status != GL_FRAMEBUFFER_COMPLETE);
            LOG_E(TAG, "check frame buffer status error - %x", status);
        }
        CHECK(glBindFramebuffer(GL_FRAMEBUFFER, 0));
    }
} // namespace sky::gles
