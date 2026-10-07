//
// Created on 2026/04/02.
//

#pragma once

#include <atomic>
#include <aurora/rhi/Fence.h>
#include <aurora/rhi/Semaphore.h>
#include <cstdint>
#include <mutex>

namespace sky::aurora {

    class MetalDevice;

    class MetalFence : public Fence {
    public:
        explicit MetalFence(MetalDevice &dev);
        ~MetalFence() override;

        bool Init(const Descriptor &desc);

        void Wait() override;
        void Reset() override;
        bool IsSignaled() override;
        bool WaitFor(uint64_t timeoutNs) override;

        // Backend-only: queue calls this on the last cmdbuffer of a Submit so
        // the GPU's completion handler signals the host-visible state.
        void *GetSharedEvent() const
        {
            return sharedEvent;
        }
        uint64_t TakeNextValue();

    private:
        MetalDevice          &device;
        void                 *sharedEvent = nullptr; // id<MTLSharedEvent>
        std::mutex            mutex;
        bool                  signaled = true;
        std::atomic<uint64_t> nextValue{0};
        // GPU value the host is currently waiting on (set by TakeNextValue at
        // Submit). Waiting uses MTLSharedEvent::waitUntilSignaledValue directly
        // so the wait does not depend on a listener object's lifetime.
        std::atomic<uint64_t> pendingValue{0};
    };

    class MetalSemaphore : public Semaphore {
    public:
        explicit MetalSemaphore(MetalDevice &dev);
        ~MetalSemaphore() override;

        bool Init(const Descriptor &desc);

        SemaphoreType GetType() const override
        {
            return type;
        }
        void     Signal(uint64_t value) override;
        bool     Wait(uint64_t value, uint64_t timeoutNs) override;
        uint64_t GetCurrentValue() const override;

        // Backend-only:
        void *GetSharedEvent() const
        {
            return sharedEvent;
        }

        // Binary helpers: each Submit-signal advances internal counter by 1;
        // the matching Submit-wait reads it.
        uint64_t AdvanceBinarySignalValue();
        uint64_t GetBinaryWaitValue() const;

    private:
        MetalDevice          &device;
        void                 *sharedEvent = nullptr; // id<MTLSharedEvent>
        SemaphoreType         type        = SemaphoreType::BINARY;
        std::atomic<uint64_t> binaryValue{0}; // implicit value for binary
    };

} // namespace sky::aurora
