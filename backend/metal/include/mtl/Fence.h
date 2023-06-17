//
// Created by Zach Lee on 2023/5/28.
//

#pragma once

#include <rhi/Fence.h>
#include <mtl/DevObject.h>
#import <Metal/Metal.h>

namespace sky::mtl {

    class Fence : public rhi::Fence, public DevObject {
    public:
        Fence(Device &dev) : DevObject(dev) {}
        ~Fence();

        void Wait() override;
        void Reset() override;
        void Signal(id<MTLCommandBuffer> commandBuffer) const;

    private:
        friend class Device;

        bool Init(const Descriptor &desc);

        id<MTLSharedEvent> event = nil;
        uint64_t pendingValue = 0;
    };

} // namespace sky::mtl
