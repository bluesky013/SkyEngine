//
// Created on 2026/04/02.
//

#include <MetalSync.h>

#include "MetalSharedEvent.h"
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <MetalDevice.h>
#include <chrono>
#include <core/logger/Logger.h>
#include <thread>

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
        if (submittedCommand)
            (void)(__bridge_transfer NSArray *)submittedCommand;
        ReleaseMetalSharedEvent(sharedEvent);
    }

    const Device *MetalFence::GetDevice() const noexcept
    {
        return &device;
    }

    bool MetalFence::Init(const Descriptor &desc)
    {
        pendingValue = desc.createSignaled ? 0 : 1;
        return CreateMetalSharedEvent(device, 0, sharedEvent);
    }

    void MetalFence::Wait()
    {
        WaitFor(UINT64_MAX);
    }

    void MetalFence::Reset()
    {
        const auto result = TryReset();
        if (!result.Succeeded())
            LOG_E("AuroraMetal", "cannot reset a submitted fence");
    }

    bool MetalFence::IsSignaled()
    {
        return Query().status == FenceStatus::SIGNALED;
    }

    bool MetalFence::WaitFor(uint64_t timeoutNs)
    {
        const auto start = std::chrono::steady_clock::now();
        for (;;) {
            const auto result = Query();
            if (result.status != FenceStatus::PENDING)
                return result.status == FenceStatus::SIGNALED;
            if (timeoutNs != UINT64_MAX &&
                static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count()) >=
                    timeoutNs)
                return false;
            std::this_thread::sleep_for(std::chrono::microseconds(50));
        }
    }

    GpuOperationResult MetalFence::TryReset() noexcept
    {
        const auto state = Query();
        if (state.status == FenceStatus::FAILED)
            return {state.error};
        return TakeNextValue() ? GpuOperationResult{} : GpuOperationResult{{GpuErrorCode::INVALID_ARGUMENT}};
    }

    FenceQueryResult MetalFence::QuerySubmitted() const
    {
        bool pending = false;
        if (submittedCommand)
            for (id<MTLCommandBuffer> command in (__bridge NSArray *)submittedCommand) {
                if (command.status == MTLCommandBufferStatusError)
                    return {FenceStatus::FAILED, {GpuErrorCode::NATIVE_ERROR, command.error ? command.error.code : 0}};
                pending |= command.status != MTLCommandBufferStatusCompleted;
            }
        return {pending ? FenceStatus::PENDING : FenceStatus::SIGNALED};
    }
    FenceQueryResult MetalFence::Query() noexcept
    {
        std::lock_guard<std::mutex> lock(mutex);
        const auto                  submitted = QuerySubmitted();
        if (submitted.status != FenceStatus::SIGNALED)
            return submitted;
        id<MTLSharedEvent> event = (__bridge id<MTLSharedEvent>)sharedEvent;
        if (event)
            return {event.signaledValue >= pendingValue.load() ? FenceStatus::SIGNALED : FenceStatus::PENDING};
        return {FenceStatus::FAILED, {GpuErrorCode::NATIVE_ERROR}};
    }

    GpuOperationResult MetalFence::ArmSubmission(void *commands, uint64_t &value)
    {
        std::lock_guard<std::mutex> lock(mutex);
        const auto                  submitted = QuerySubmitted();
        if (submitted.status == FenceStatus::FAILED)
            return {submitted.error};
        if (!commands || submitted.status == FenceStatus::PENDING || nextValue == UINT64_MAX)
            return {{GpuErrorCode::INVALID_ARGUMENT}};
        if (submittedCommand)
            (void)(__bridge_transfer NSArray *)submittedCommand;
        submittedCommand = (__bridge_retained void *)(__bridge NSArray *)commands;
        value            = ++nextValue;
        pendingValue     = value;
        return {};
    }

    uint64_t MetalFence::TakeNextValue()
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (QuerySubmitted().status != FenceStatus::SIGNALED || nextValue == UINT64_MAX)
            return 0;
        if (submittedCommand) {
            (void)(__bridge_transfer NSArray *)submittedCommand;
            submittedCommand = nullptr;
        }
        pendingValue = ++nextValue;
        return pendingValue;
    }

    // ---- MetalSemaphore ---------------------------------------------------------

    MetalSemaphore::MetalSemaphore(MetalDevice &dev) : device(dev)
    {
    }

    MetalSemaphore::~MetalSemaphore()
    {
        ReleaseMetalSharedEvent(sharedEvent);
    }

    const Device *MetalSemaphore::GetDevice() const noexcept
    {
        return &device;
    }

    bool MetalSemaphore::Init(const Descriptor &desc)
    {
        type                = desc.type;
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
        if (event == nil)
            return false;
        const auto start = std::chrono::steady_clock::now();
        while (event.signaledValue < value) {
            if (timeoutNs != UINT64_MAX &&
                static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count()) >=
                    timeoutNs)
                return false;
            std::this_thread::sleep_for(std::chrono::microseconds(50));
        }
        return true;
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
