//
// Created by Zach Lee on 2026/3/29.
//

#pragma once

#include <core/template/ReferenceObject.h>
#include <cstdint>

namespace sky::aurora {

    class Device;

    enum class SemaphoreType : uint32_t {
        BINARY   = 0,
        TIMELINE = 1,
    };

    class Semaphore : public RefObject {
    public:
        // Unknown owners cannot enter backend-specific native paths.
        virtual const Device *GetDevice() const noexcept
        {
            return nullptr;
        }

        struct Descriptor {
            SemaphoreType type         = SemaphoreType::BINARY;
            uint64_t      initialValue = 0; // timeline only
        };

        Semaphore()           = default;
        ~Semaphore() override = default;

        virtual SemaphoreType GetType() const = 0;

        // Timeline-only host operations. Calling these on a binary semaphore
        // is undefined; debug builds may assert.
        virtual void     Signal(uint64_t value)                   = 0;
        virtual bool     Wait(uint64_t value, uint64_t timeoutNs) = 0;
        virtual uint64_t GetCurrentValue() const                  = 0;
    };

    using SemaphorePtr = CounterPtr<Semaphore>;

} // namespace sky::aurora
