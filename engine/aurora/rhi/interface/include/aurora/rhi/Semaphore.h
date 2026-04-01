//
// Created by Zach Lee on 2026/3/29.
//

#pragma once

#include <cstdint>
#include <core/template/ReferenceObject.h>

namespace sky::aurora {

    class Semaphore : public RefObject {
    public:
        struct Descriptor {
            uint64_t initialValue = 0;
        };

        Semaphore() = default;
        ~Semaphore() override = default;
    };

    using SemaphorePtr = CounterPtr<Semaphore>;

} // namespace sky::aurora
