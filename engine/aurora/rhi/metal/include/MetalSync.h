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

        uint64_t GetInitialValue() const { return initialValue; }

    private:
        uint64_t initialValue = 0;
    };

} // namespace sky::aurora