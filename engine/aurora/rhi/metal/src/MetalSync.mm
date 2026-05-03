//
// Created on 2026/04/02.
//

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <MetalSync.h>
#include <MetalDevice.h>
#include <chrono>

namespace sky::aurora {

    // ---- MetalFence -------------------------------------------------------------

    MetalFence::MetalFence(MetalDevice &dev)
        : device(dev)
    {
    }

    MetalFence::~MetalFence()
    {
        if (sharedEvent != nullptr) {
            id<MTLSharedEvent> e = (__bridge_transfer id<MTLSharedEvent>)sharedEvent;
            e = nil;
            sharedEvent = nullptr;
        }
    }

    bool MetalFence::Init(const Descriptor &desc)
    {
        signaled = desc.createSignaled;

        auto *metalDevice = (__bridge id<MTLDevice>)device.GetNativeDevice();
        id<MTLSharedEvent> event = [metalDevice newSharedEvent];
        if (event == nil) {
            return false;
        }
        sharedEvent = (__bridge_retained void *)event;
        return true;
    }

    void MetalFence::Wait()
    {
        std::unique_lock<std::mutex> lock(mutex);
        condition.wait(lock, [this]() { return signaled; });
    }

    void MetalFence::Reset()
    {
        std::lock_guard<std::mutex> lock(mutex);
        signaled = false;
    }

    bool MetalFence::IsSignaled()
    {
        std::lock_guard<std::mutex> lock(mutex);
        return signaled;
    }

    bool MetalFence::WaitFor(uint64_t timeoutNs)
    {
        std::unique_lock<std::mutex> lock(mutex);
        return condition.wait_for(lock, std::chrono::nanoseconds(timeoutNs), [this]() { return signaled; });
    }

    uint64_t MetalFence::TakeNextValue()
    {
        const uint64_t v = ++nextValue;

        // Reset signaled state since we expect the GPU to re-signal at this value.
        {
            std::lock_guard<std::mutex> lock(mutex);
            signaled = false;
        }

        // Register notify listener that will flip signaled=true when GPU reaches v.
        id<MTLSharedEvent> event = (__bridge id<MTLSharedEvent>)sharedEvent;
        MTLSharedEventListener *listener = [[[MTLSharedEventListener alloc] init] autorelease];
        [event notifyListener:listener atValue:v block:^(id<MTLSharedEvent>, uint64_t) {
            this->Signal();
        }];

        return v;
    }

    void MetalFence::Signal()
    {
        {
            std::lock_guard<std::mutex> lock(mutex);
            signaled = true;
        }
        condition.notify_all();
    }

    // ---- MetalSemaphore ---------------------------------------------------------

    MetalSemaphore::MetalSemaphore(MetalDevice &dev)
        : device(dev)
    {
    }

    MetalSemaphore::~MetalSemaphore()
    {
        if (sharedEvent != nullptr) {
            id<MTLSharedEvent> e = (__bridge_transfer id<MTLSharedEvent>)sharedEvent;
            e = nil;
            sharedEvent = nullptr;
        }
    }

    bool MetalSemaphore::Init(const Descriptor &desc)
    {
        type = desc.type;

        auto *metalDevice = (__bridge id<MTLDevice>)device.GetNativeDevice();
        id<MTLSharedEvent> event = [metalDevice newSharedEvent];
        if (event == nil) {
            return false;
        }
        if (type == SemaphoreType::TIMELINE && desc.initialValue > 0) {
            event.signaledValue = desc.initialValue;
        }
        sharedEvent = (__bridge_retained void *)event;
        return true;
    }

    void MetalSemaphore::Signal(uint64_t value)
    {
        // Timeline-only host signal; binary semaphores ignore.
        if (type != SemaphoreType::TIMELINE) {
            return;
        }
        id<MTLSharedEvent> event = (__bridge id<MTLSharedEvent>)sharedEvent;
        if (value > event.signaledValue) {
            event.signaledValue = value;
        }
    }

    bool MetalSemaphore::Wait(uint64_t value, uint64_t timeoutNs)
    {
        if (type != SemaphoreType::TIMELINE) {
            return false;
        }
        id<MTLSharedEvent> event = (__bridge id<MTLSharedEvent>)sharedEvent;
        return [event waitUntilSignaledValue:value timeoutMS:timeoutNs / 1'000'000ULL] == YES;
    }

    uint64_t MetalSemaphore::GetCurrentValue() const
    {
        id<MTLSharedEvent> event = (__bridge id<MTLSharedEvent>)sharedEvent;
        return event.signaledValue;
    }

    uint64_t MetalSemaphore::AdvanceBinarySignalValue()
    {
        return ++binaryValue;
    }

    uint64_t MetalSemaphore::GetBinaryWaitValue() const
    {
        return binaryValue.load();
    }

} // namespace sky::aurora
