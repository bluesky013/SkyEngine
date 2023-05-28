//
// Created by Zach Lee on 2023/5/28.
//

#pragma once

#include <rhi/Semaphore.h>
#include <mtl/DevObject.h>
#import <Metal/MTLEvent.h>

namespace sky::mtl {
    class Device;

    class Semaphore : public rhi::Semaphore, public DevObject {
    public:
        Semaphore(Device &dev) : DevObject(dev) {}
        ~Semaphore();

    private:
        friend class Device;
        bool Init(const Descriptor &desc);

        id<MTLEvent> event;
    };

} // namespace sky::mtl
