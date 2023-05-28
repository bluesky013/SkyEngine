//
// Created by Zach Lee on 2023/5/28.
//

#pragma once

#include <rhi/RenderPass.h>
#include <mtl/DevObject.h>
#import <Metal/MTLPixelFormat.h>

namespace sky::mtl {
    class Device;

    struct Attachment {
        MTLPixelFormat format;
    };

    class RenderPass : public rhi::RenderPass, public DevObject {
    public:
        RenderPass(Device &dev) : DevObject(dev) {}
        ~RenderPass() = default;

    private:
        friend class Device;

        bool Init(const Descriptor &desc);

        std::vector<Attachment> attachments;
    };

} // namespace sky::mtl
