//
// Created by Zach Lee on 2026/3/29.
//

#pragma once

#include <cstdint>
#include <core/template/ReferenceObject.h>

namespace sky::aurora {

    enum class SemaphoreType : uint32_t {
        BINARY   = 0,
        TIMELINE = 1,
    };

    class Semaphore : public RefObject {
    public:
        struct Descriptor {
            SemaphoreType type         = SemaphoreType::BINARY;
            uint64_t      initialValue = 0;                 // timeline only
        };

        Semaphore() = default;
        ~Semaphore() override = default;

        virtual SemaphoreType GetType() const = 0;

        // Timeline-only host operations. Calling these on a binary semaphore
        // is undefined; debug builds may assert.
        virtual void     Signal(uint64_t value) = 0;
        virtual bool     Wait(uint64_t value, uint64_t timeoutNs) = 0;
        virtual uint64_t GetCurrentValue() const = 0;
    };

    using SemaphorePtr = CounterPtr<Semaphore>;

} // namespace sky::aurora
