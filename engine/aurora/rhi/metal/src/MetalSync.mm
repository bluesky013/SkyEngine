//
// Created on 2026/04/02.
//

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <MetalSync.h>

#include <MetalDevice.h>
#include "MetalSharedEvent.h"

namespace sky::aurora {

    bool CreateMetalSharedEvent(MetalDevice &device, uint64_t initialValue, void *&outEvent)
    {
        auto              *metalDevice = (__bridge id<MTLDevice>)device.GetNativeDevice();
        id<MTLSharedEvent> event       = [metalDevice newSharedEvent];
        if (event == nil) {
            return false;
        }
        if (initialValue > 0) {
            event.signaledValue = initialValue;
        }
        outEvent = (__bridge_retained void *)event;
        return true;
    }

    void ReleaseMetalSharedEvent(void *&event)
    {
        if (event != nullptr) {
            (void)(__bridge_transfer id<MTLSharedEvent>)event;
            event = nullptr;
        }
    }

    // ---- MetalFence -------------------------------------------------------------

    MetalFence::MetalFence(MetalDevice &dev) : device(dev)
    {
    }

    MetalFence::~MetalFence()
    {
        ReleaseMetalSharedEvent(sharedEvent);
    }

    bool MetalFence::Init(const Descriptor &desc)
    {
        signaled     = desc.createSignaled;
        pendingValue = desc.createSignaled ? 0 : 1;
        return CreateMetalSharedEvent(device, 0, sharedEvent);
    }

    void MetalFence::Wait()
    {
        id<MTLSharedEvent> event = (__bridge id<MTLSharedEvent>)sharedEvent;
        if (event == nil) {
            return;
        }
        // Block on the GPU value directly; robust across autorelease-pool drains
        // (no listener object that could be deallocated before it fires).
        [event waitUntilSignaledValue:pendingValue.load() timeoutMS:UINT64_MAX];

        std::lock_guard<std::mutex> lock(mutex);
        signaled = true;
    }

    void MetalFence::Reset()
    {
        std::lock_guard<std::mutex> lock(mutex);
        signaled = false;
    }

    bool MetalFence::IsSignaled()
    {
        id<MTLSharedEvent> event = (__bridge id<MTLSharedEvent>)sharedEvent;
        if (event != nil) {
            return event.signaledValue >= pendingValue.load();
        }
        std::lock_guard<std::mutex> lock(mutex);
        return signaled;
    }

    bool MetalFence::WaitFor(uint64_t timeoutNs)
    {
        id<MTLSharedEvent> event = (__bridge id<MTLSharedEvent>)sharedEvent;
        if (event == nil) {
            return signaled;
        }
        const bool ok = [event waitUntilSignaledValue:pendingValue.load() timeoutMS:timeoutNs / 1'000'000ULL] == YES;
        if (ok) {
            std::lock_guard<std::mutex> lock(mutex);
            signaled = true;
        }
        return ok;
    }

    GpuOperationResult MetalFence::TryReset() noexcept
    {
        const auto state = Query();
        if (state.status == FenceStatus::FAILED)
            return {state.error};
        if (state.status == FenceStatus::PENDING)
            return {{GpuErrorCode::NATIVE_ERROR}};
        // The next Submit advances the shared-event target again; skipping one
        // value here is harmless and keeps reset from blocking the CPU.
        TakeNextValue();
        return {};
    }

    FenceQueryResult MetalFence::Query() noexcept
    {
        id<MTLSharedEvent> event = (__bridge id<MTLSharedEvent>)sharedEvent;
        if (event != nil)
            return {event.signaledValue >= pendingValue.load() ? FenceStatus::SIGNALED : FenceStatus::PENDING};
        std::lock_guard<std::mutex> lock(mutex);
        return {signaled ? FenceStatus::SIGNALED : FenceStatus::PENDING};
    }

    uint64_t MetalFence::TakeNextValue()
    {
        const uint64_t v = ++nextValue;

        {
            std::lock_guard<std::mutex> lock(mutex);
            signaled = false;
        }
        pendingValue = v;
        return v;
    }

    // ---- MetalSemaphore ---------------------------------------------------------

    MetalSemaphore::MetalSemaphore(MetalDevice &dev) : device(dev)
    {
    }

    MetalSemaphore::~MetalSemaphore()
    {
        ReleaseMetalSharedEvent(sharedEvent);
    }

    bool MetalSemaphore::Init(const Descriptor &desc)
    {
        type               = desc.type;
        const uint64_t init = (type == SemaphoreType::TIMELINE) ? desc.initialValue : 0;
        return CreateMetalSharedEvent(device, init, sharedEvent);
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
