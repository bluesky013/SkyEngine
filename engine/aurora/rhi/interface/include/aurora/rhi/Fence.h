//
// Created by Zach Lee on 2026/3/29.
//

#pragma once

#include <cstdint>
#include <core/template/ReferenceObject.h>

namespace sky::aurora {

    class Fence : public RefObject {
    public:
        struct Descriptor {
            bool createSignaled = true;
        };

        Fence() = default;
        ~Fence() override = default;

        virtual void Wait() = 0;
        virtual void Reset() = 0;

        void WaitAndReset()
        {
            Wait();
            Reset();
        }
    };

    using FencePtr = CounterPtr<Fence>;

} // namespace sky::aurora
