//
// Created by Zach Lee on 2023/5/28.
//

#pragma once

#include <rhi/Semaphore.h>
#include <mtl/DevObject.h>
#import <Metal/MTLEvent.h>
#import <Metal/MTLCommandBuffer.h>

namespace sky::mtl {
    class Device;

    class Semaphore : public rhi::Semaphore, public DevObject {
    public:
        Semaphore(Device &dev) : DevObject(dev) {}
        ~Semaphore();

        void Signal(id<MTLCommandBuffer> commandBuffer);
        void Wait(id<MTLCommandBuffer> commandBuffer);

    private:
        friend class Device;
        bool Init(const Descriptor &desc);

        id<MTLEvent> event = nil;
        uint32_t currentCounter = 0;
    };

} // namespace sky::mtl
