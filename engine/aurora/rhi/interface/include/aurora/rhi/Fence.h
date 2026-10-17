//
// Created by Zach Lee on 2026/3/29.
//

#pragma once

#include <aurora/rhi/SubmissionResult.h>
#include <core/template/ReferenceObject.h>
#include <cstdint>

namespace sky::aurora {

    class Device;

    class Fence : public RefObject {
    public:
        // Unknown owners cannot enter backend-specific native paths.
        virtual const Device *GetDevice() const noexcept
        {
            return nullptr;
        }

        struct Descriptor {
            bool createSignaled = true;
        };

        Fence()           = default;
        ~Fence() override = default;

        virtual void Wait()                      = 0;
        virtual void Reset()                     = 0;
        virtual bool IsSignaled()                = 0;
        virtual bool WaitFor(uint64_t timeoutNs) = 0;

        virtual FenceQueryResult Query() noexcept
        {
            return {FenceStatus::FAILED, {GpuErrorCode::UNSUPPORTED}};
        }
        // Only reset an idle fence immediately before a submission attempt; reset itself does not imply GPU work.
        virtual GpuOperationResult TryReset() noexcept
        {
            return {{GpuErrorCode::UNSUPPORTED}};
        }

        void WaitAndReset()
        {
            Wait();
            Reset();
        }
    };

    using FencePtr = CounterPtr<Fence>;

} // namespace sky::aurora
