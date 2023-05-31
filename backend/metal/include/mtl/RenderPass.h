//
// Created by Zach Lee on 2023/5/28.
//

#pragma once

#include <rhi/RenderPass.h>
#include <rhi/Decode.h>
#include <mtl/DevObject.h>
#import <Metal/MTLPixelFormat.h>
#import <Metal/MTLRenderPass.h>

namespace sky::mtl {
    class Device;

    struct MTLAttachment {
        MTLPixelFormat format;
        MTLLoadAction load;
        MTLStoreAction store;
    };

    class RenderPass : public rhi::RenderPass, public DevObject {
    public:
        RenderPass(Device &dev) : DevObject(dev) {}
        ~RenderPass() = default;

        bool HasDepth() const { return hasDepth; }
        bool HasStencil() const { return hasStencil; }
        const std::vector<MTLAttachment> &GetColorAttachments() const { return colorAttachments; }
        const MTLAttachment &GetDepthAttachment() const { return depthAttachment; }
        const MTLAttachment &GetStencilAttachment() const { return stencilAttachment; }
        NSUInteger GetSamplerCount() const { return samplerCount; }

    private:
        friend class Device;

        bool Init(const Descriptor &desc);

        std::vector<MTLAttachment> colorAttachments;
        std::vector<MTLAttachment> resolveAttachments;
        MTLAttachment depthAttachment;
        MTLAttachment stencilAttachment;
        bool hasDepth = false;
        bool hasStencil = false;
        NSUInteger samplerCount = 1;
    };
    using RenderPassPtr = std::shared_ptr<RenderPass>;

} // namespace sky::mtl
