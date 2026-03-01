//
// Created by Zach Lee on 2023/5/28.
//

#include <mtl/Semaphore.h>
#include <mtl/Device.h>

namespace sky::mtl {

    Semaphore::~Semaphore()
    {
        if (event) {
            [event release];
            event = nil;
        }
    }

    bool Semaphore::Init(const Descriptor &desc)
    {
        event = [device.GetMetalDevice() newEvent];
        return event != nil;
    }

    void Semaphore::Signal(id<MTLCommandBuffer> commandBuffer)
    {
        ++currentCounter;
        [commandBuffer encodeSignalEvent: event value: currentCounter];
    }

    void Semaphore::Wait(id<MTLCommandBuffer> commandBuffer)
    {
        [commandBuffer encodeWaitForEvent: event value: currentCounter];
    }


} // namespace sky::mtl
