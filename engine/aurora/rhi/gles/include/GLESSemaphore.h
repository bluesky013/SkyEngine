//
// Created on 2026/04/01.
//

#pragma once

#include <aurora/rhi/Semaphore.h>

namespace sky::aurora {

    // GLES has no native timeline semaphore; emulate with CPU-side counter.
    class GLESSemaphore : public Semaphore {
    public:
        GLESSemaphore() = default;
        ~GLESSemaphore() override = default;

        bool Init(const Descriptor &desc);

        SemaphoreType GetType() const override { return type; }
        void          Signal(uint64_t value) override;
        bool          Wait(uint64_t value, uint64_t timeoutNs) override;
        uint64_t      GetCurrentValue() const override { return counter; }

    private:
        SemaphoreType type    = SemaphoreType::BINARY;
        uint64_t      counter = 0;
    };

} // namespace sky::aurora
