//
// Created on 2026/04/01.
//

#pragma once

#include <aurora/rhi/Semaphore.h>
#include <condition_variable>
#include <mutex>

namespace sky::aurora {

    // GLES has no native timeline semaphore; emulate with CPU-side counter
    // backed by a mutex + condvar so Submit-time wait/signal can block.
    class GLESSemaphore : public Semaphore {
    public:
        GLESSemaphore() = default;
        ~GLESSemaphore() override = default;

        bool Init(const Descriptor &desc);

        SemaphoreType GetType() const override { return type; }
        void          Signal(uint64_t value) override;
        bool          Wait(uint64_t value, uint64_t timeoutNs) override;
        uint64_t      GetCurrentValue() const override;

    private:
        SemaphoreType type    = SemaphoreType::BINARY;
        mutable std::mutex      mutex;
        std::condition_variable condition;
        uint64_t      counter = 0;
    };

} // namespace sky::aurora
