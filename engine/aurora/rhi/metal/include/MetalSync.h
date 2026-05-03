//
// Created on 2026/04/02.
//

#pragma once

#include <aurora/rhi/Fence.h>
#include <aurora/rhi/Semaphore.h>
#include <condition_variable>
#include <mutex>

namespace sky::aurora {

    class MetalFence : public Fence {
    public:
        MetalFence() = default;
        ~MetalFence() override = default;

        bool Init(const Descriptor &desc);

        void Wait() override;
        void Reset() override;
        bool IsSignaled() override;
        bool WaitFor(uint64_t timeoutNs) override;

    private:
        std::mutex              mutex;
        std::condition_variable condition;
        bool                    signaled = true;
    };

    class MetalSemaphore : public Semaphore {
    public:
        MetalSemaphore() = default;
        ~MetalSemaphore() override = default;

        bool Init(const Descriptor &desc);

        SemaphoreType GetType() const override { return type; }
        void          Signal(uint64_t value) override;
        bool          Wait(uint64_t value, uint64_t timeoutNs) override;
        uint64_t      GetCurrentValue() const override { return currentValue; }

    private:
        SemaphoreType         type         = SemaphoreType::BINARY;
        uint64_t              initialValue = 0;
        uint64_t              currentValue = 0;
    };

} // namespace sky::aurora