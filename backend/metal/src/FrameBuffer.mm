//
// Created by Zach Lee on 2023/5/28.
//

#include <mtl/FrameBuffer.h>
#include <mtl/Device.h>
#include <mtl/Conversion.h>

namespace sky::mtl {

    FrameBuffer::~FrameBuffer()
    {
    }

    bool FrameBuffer::Init(const Descriptor &desc)
    {
        attachments.reserve(desc.views.size());
        for (auto &view : desc.views) {
            attachments.emplace_back(std::static_pointer_cast<ImageView>(view));
        }
        return true;
    }

    MTLRenderPassDescriptor *FrameBuffer::RequestRenderPassDescriptor(const RenderPassPtr &pass, uint32_t clearCount,
                                                           rhi::ClearValue *clears)
    {
        MTLRenderPassDescriptor* passDesc = [MTLRenderPassDescriptor renderPassDescriptor];
        auto &colors = pass->GetColors();
        auto &resolves = pass->GetResolves();
        auto &colorAttachments = pass->GetColorAttachments();
        bool hasResolve = !resolves.empty();
        
        for (uint32_t i = 0; i < colors.size(); ++i) {
            auto &imageView     = attachments[colors[i]];
            auto &clearValue = clears[colors[i]].color;

            passDesc.colorAttachments[i].loadAction = colorAttachments[i].load;
            passDesc.colorAttachments[i].storeAction = colorAttachments[i].store;
            passDesc.colorAttachments[i].texture = imageView->GetNativeHandle();
            passDesc.colorAttachments[i].clearColor = MTLClearColorMake(clearValue.float32[0],
                                                         clearValue.float32[1],
                                                         clearValue.float32[2],
                                                         clearValue.float32[3]);
            if (hasResolve) {
                auto &resolveView = attachments[resolves[i]];
                passDesc.colorAttachments[i].resolveTexture = resolveView->GetNativeHandle();
                passDesc.colorAttachments[i].resolveLevel = 0;
                passDesc.colorAttachments[i].resolveSlice = 0;
            }
        }

        if (pass->HasDepth()) {
            auto &clearValue = clears[pass->GetDepthStencil()].depthStencil;
            auto &imageView = attachments[pass->GetDepthStencil()];
            const auto &depthAttachment = pass->GetDepthAttachment();
            passDesc.depthAttachment.texture = imageView->GetNativeHandle();
            passDesc.depthAttachment.loadAction = depthAttachment.load;
            passDesc.depthAttachment.storeAction = depthAttachment.store;
            passDesc.depthAttachment.clearDepth = clearValue.depth;
        }
        if (pass->HasStencil()) {
            auto &clearValue = clears[pass->GetDepthStencil()].depthStencil;
            auto &imageView = attachments[pass->GetDepthStencil()];
            const auto &stencilAttachment = pass->GetStencilAttachment();
            passDesc.stencilAttachment.texture = imageView->GetNativeHandle();
            passDesc.stencilAttachment.loadAction = stencilAttachment.load;
            passDesc.stencilAttachment.storeAction = stencilAttachment.store;
            passDesc.stencilAttachment.clearStencil = clearValue.stencil;
        }
        return passDesc;
    }

} // namespace sky::mtl
