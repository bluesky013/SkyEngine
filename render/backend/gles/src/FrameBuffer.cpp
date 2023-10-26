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
        framebuffer = nullptr;
        resolveFramebuffer = nullptr;
    }

    bool FrameBuffer::Init(const Descriptor &fbDesc)
    {
        extent = fbDesc.extent;
        renderPass = std::static_pointer_cast<RenderPass>(fbDesc.pass);

        attachments.reserve(fbDesc.views.size());
        for (auto &attachment : fbDesc.views) {
            attachments.emplace_back(std::static_pointer_cast<ImageView>(attachment));
        }

//        uint32_t count = device.GetInternalFeature().msaa1 ? (device.GetInternalFeature().msaa2 ? 255 : 1) : 0;
        uint32_t count = 1;

        const auto &attDescriptions = renderPass->GetAttachments();
        const auto &colors = renderPass->GetColors();
        const auto &colorIndices = renderPass->GetAttachmentColorMap();
        const auto &colorResolves = renderPass->GetResolves();
        auto depthStencil = renderPass->GetDepthStencil();
        auto depthStencilResolve = renderPass->GetDepthStencilResolve();

        uint32_t colorNum = static_cast<uint32_t>(colors.size());
        framebuffer = std::make_unique<FramebufferObject>(device);
        resolveFramebuffer = std::make_unique<FramebufferObject>(device);

        uint32_t resolveIndex = 0;
        for (uint32_t i = 0; i < colorNum; ++i) {
            const auto &attachmentIndex        = colors[i];
            const auto &colorIndex             = colorIndices[attachmentIndex];
            const auto &resolveAttachmentIndex = colorResolves.empty() ? INVALID_INDEX : colorResolves[i];

            const auto &desc = attDescriptions[attachmentIndex];
            const auto &view = attachments[attachmentIndex];
            auto &imageDesc = view->GetImage()->GetDescriptor();

            if (desc.sample != rhi::SampleCount::X1 && resolveAttachmentIndex != INVALID_INDEX) {
                const auto &resolveDesc = attDescriptions[resolveAttachmentIndex];
                const auto &resolveView = attachments[resolveAttachmentIndex];

                if (imageDesc.usage & rhi::ImageUsageFlagBit::TRANSIENT && // is memoryless resource
                    !view->GetImage()->GetSurface() &&                     // not back buffer
                    i < count) {                                           // ext limit
                    framebuffer->BindColor(resolveView, resolveDesc, colorIndex, desc.sample);
                } else {
                    colorBlitPairs.emplace_back(std::pair{colorIndex, resolveIndex});
                    framebuffer->BindColor(view, desc, colorIndex, rhi::SampleCount::X1);
                    resolveFramebuffer->BindColor(resolveView, resolveDesc, resolveIndex, rhi::SampleCount::X1);
                    ++resolveIndex;
                }
            } else {
                framebuffer->BindColor(view, desc, colorIndex, rhi::SampleCount::X1);
            }
        }

        if (depthStencil != INVALID_INDEX) {
            const auto &desc = attDescriptions[depthStencil];
            const auto &view = attachments[depthStencil];
            auto &imageDesc = view->GetImage()->GetDescriptor();

            if (desc.sample != rhi::SampleCount::X1 && depthStencilResolve != INVALID_INDEX) {
                const auto &resolveDesc = attDescriptions[depthStencilResolve];
                const auto &resolveView = attachments[depthStencilResolve];

                if (imageDesc.usage & rhi::ImageUsageFlagBit::TRANSIENT && // is memoryless resource
                    !view->GetImage()->GetSurface() &&                     // not back buffer
                    depthStencil < count) {                                // ext limit
                    framebuffer->BindDepthStencil(resolveView, resolveDesc, desc.sample);
                } else {
                    dsResolveMask |= HasDepth(resolveDesc.format) ? GL_DEPTH_BUFFER_BIT : 0;
                    dsResolveMask |= HasStencil(resolveDesc.format) ? GL_STENCIL_BUFFER_BIT : 0;

                    framebuffer->BindDepthStencil(view, desc, rhi::SampleCount::X1);
                    resolveFramebuffer->BindDepthStencil(resolveView, resolveDesc, rhi::SampleCount::X1);
                }
            } else {
                framebuffer->BindDepthStencil(view, desc, rhi::SampleCount::X1);
            }
        }

        return true;
    }

    FramebufferObject *FrameBuffer::GetFbo() const
    {
        return framebuffer.get();
    }

    bool FrameBuffer::NeedResolve() const
    {
        return !colorBlitPairs.empty() || dsResolveMask != 0;
    }

    void FrameBuffer::DoResolve(Context &context, uint32_t queueIndex) const
    {
        auto srcFb = framebuffer->AcquireNativeHandle(context, queueIndex);
        auto dstFb = resolveFramebuffer->AcquireNativeHandle(context, queueIndex);
        CHECK(glBindFramebuffer(GL_READ_FRAMEBUFFER, srcFb));
        CHECK(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dstFb));

        if (!colorBlitPairs.empty()) {
            auto resolveColorNum = resolveFramebuffer->colorResolves.size();
            std::vector<GLenum> drawBuffers(resolveColorNum, GL_NONE);
            for (auto &[src, dst] : colorBlitPairs) {
                drawBuffers[dst] = GL_COLOR_ATTACHMENT0 + dst;
                CHECK(glReadBuffer(GL_COLOR_ATTACHMENT0 + src));
                CHECK(glDrawBuffers(static_cast<GLsizei>(resolveColorNum), drawBuffers.data()));

                CHECK(glBlitFramebuffer(
                    0, 0, extent.width, extent.height,
                    0, 0, extent.width, extent.height,
                    GL_COLOR_BUFFER_BIT, GL_NEAREST));
                drawBuffers[dst] = GL_NONE;
            }
        }
        if (dsResolveMask != 0) {
            CHECK(glBlitFramebuffer(
                0, 0, extent.width, extent.height,
                0, 0, extent.width, extent.height,
                dsResolveMask, GL_NEAREST));
        }

        if (!framebuffer->postInvalidates.empty()) {
            CHECK(glInvalidateFramebuffer(GL_READ_FRAMEBUFFER, static_cast<uint32_t>(framebuffer->postInvalidates.size()), framebuffer->postInvalidates.data()));
        }
        if (!resolveFramebuffer->postInvalidates.empty()) {
            CHECK(glInvalidateFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<uint32_t>(resolveFramebuffer->postInvalidates.size()), resolveFramebuffer->postInvalidates.data()));
        }

    }
}
