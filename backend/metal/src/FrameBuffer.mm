//
// Created by Zach Lee on 2023/5/28.
//

#include <mtl/FrameBuffer.h>
#include <mtl/Device.h>
#include <mtl/Conversion.h>

namespace sky::mtl {

    FrameBuffer::~FrameBuffer()
    {
        if (passDesc) {
            [passDesc release];
        }
    }

    bool FrameBuffer::Init(const Descriptor &desc)
    {
        attachments.reserve(desc.views.size());
        for (auto &view : desc.views) {
            attachments.emplace_back(std::static_pointer_cast<ImageView>(view));
        }

        auto pass = std::static_pointer_cast<RenderPass>(desc.pass);
        passDesc = [[MTLRenderPassDescriptor alloc] init];

        auto &colors = pass->GetAttachmentColorMap();
        auto &resolves = pass->GetResolves();
        auto &colorAttachments = pass->GetColorAttachments();
        bool hasResolve = !resolves.empty();

        for (uint32_t i = 0; i < colors.size(); ++i) {
            auto &imageView = attachments[colors[i]];
            auto passColorDesc = passDesc.colorAttachments[i];

            passColorDesc.texture = imageView->GetNativeHandle();
            passColorDesc.level = 0;
            passColorDesc.slice = 0;
            passColorDesc.loadAction = colorAttachments[i].load;
            passColorDesc.storeAction = colorAttachments[i].store;

            if (hasResolve) {
                auto &resolveView = attachments[resolves[i]];
                passColorDesc.resolveTexture = resolveView->GetNativeHandle();
                passColorDesc.resolveLevel = 0;
                passColorDesc.resolveSlice = 0;
            }
        }

        if (pass->HasDepth()) {
            auto &imageView = attachments[pass->GetDepthStencil()];
            const auto &depthAttachment = pass->GetDepthAttachment();
            passDesc.depthAttachment.texture = imageView->GetNativeHandle();
            passDesc.depthAttachment.loadAction = depthAttachment.load;
            passDesc.depthAttachment.storeAction = depthAttachment.store;
        }

        if (pass->HasStencil()) {
            auto &imageView = attachments[pass->GetDepthStencil()];
            const auto &stencilAttachment = pass->GetStencilAttachment();
            passDesc.stencilAttachment.texture = imageView->GetNativeHandle();
            passDesc.stencilAttachment.loadAction = stencilAttachment.load;
            passDesc.stencilAttachment.storeAction = stencilAttachment.store;
        }

        return true;
    }

} // namespace sky::mtl
