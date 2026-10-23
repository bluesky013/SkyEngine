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

        bool          Init(const Descriptor &desc);
        const Device *GetDevice() const noexcept override;

        void               Wait() override;
        void               Reset() override;
        bool               IsSignaled() override;
        bool               WaitFor(uint64_t timeoutNs) override;
        GpuOperationResult TryReset() noexcept override;
        FenceQueryResult   Query() noexcept override;

        // Queue arming and reset reserve values independently of submission completion.
        void *GetSharedEvent() const
        {
            return sharedEvent;
        }
        uint64_t           TakeNextValue();
        GpuOperationResult ArmSubmission(void *commands, uint64_t &value);

    private:
        FenceQueryResult      QuerySubmitted() const;
        MetalDevice          &device;
        void                 *sharedEvent      = nullptr; // id<MTLSharedEvent>
        void                 *submittedCommand = nullptr; // owned NSArray<id<MTLCommandBuffer>>
        std::mutex            mutex;
        std::atomic<uint64_t> nextValue{0};
        // The native command batch must complete successfully before its event value can retire.
        std::atomic<uint64_t> pendingValue{0};
    };

    class MetalSemaphore : public Semaphore {
    public:
        explicit MetalSemaphore(MetalDevice &dev);
        ~MetalSemaphore() override;

        bool          Init(const Descriptor &desc);
        const Device *GetDevice() const noexcept override;

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
