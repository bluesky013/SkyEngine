//
// Created by Zach Lee on 2023/6/1.
//

#include <mtl/Fence.h>
#include <mtl/Device.h>

namespace sky::mtl {

    Fence::~Fence()
    {
        if (event) {
            [event release];
            event = nil;
        }
    }

    void Fence::Wait()
    {
        auto value = event.signaledValue;
        if (event.signaledValue < pendingValue) {
            auto *listener  = device.GetSharedEventListener();
            auto  semaphore = dispatch_semaphore_create(0);
            [event notifyListener:listener
                          atValue:pendingValue
                            block:^(id<MTLSharedEvent> sharedEvent, uint64 value) {
                                dispatch_semaphore_signal(semaphore);
                            }];
            dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);
            dispatch_release(semaphore);
        }
    }

    void Fence::Reset()
    {
        ++pendingValue;
    }

    bool Fence::Init(const Descriptor &desc)
    {
        event        = [device.GetMetalDevice() newSharedEvent];
        pendingValue = desc.createSignaled ? 0 : 1;
        return event != nil;
    }

    void Fence::Signal(id<MTLCommandBuffer> commandBuffer) const
    {
        [commandBuffer encodeSignalEvent:event value:pendingValue];
    }

} // namespace sky::mtl
