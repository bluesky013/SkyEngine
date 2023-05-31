//
// Created by Zach Lee on 2023/5/28.
//

#pragma once

#include <rhi/FrameBuffer.h>
#include <mtl/ImageView.h>
#include <mtl/RenderPass.h>
#include <mtl/DevObject.h>
#import <Metal/MTLRenderPass.h>

namespace sky::mtl {
    class Device;
    class FrameBuffer : public rhi::FrameBuffer, public DevObject {
    public:
        FrameBuffer(Device &dev) : DevObject(dev) {}
        ~FrameBuffer();

    private:
        friend class Device;
        bool Init(const Descriptor &desc);

        std::vector<ImageViewPtr> attachments;
        MTLRenderPassDescriptor *passDesc = nil;
    };
    using FrameBufferPtr = std::shared_ptr<FrameBuffer>;

} // namespace sky::mtl
