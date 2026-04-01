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

    private:
        uint64_t counter = 0;
    };

} // namespace sky::aurora
