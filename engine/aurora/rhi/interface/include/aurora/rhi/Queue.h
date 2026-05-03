//
// Aurora RHI Queue.
//

#pragma once

#include <aurora/rhi/Core.h>

namespace sky::aurora {

    struct SubmitInfo;

    class Queue {
    public:
        Queue() = default;
        virtual ~Queue() = default;

        virtual void      Submit(const SubmitInfo &info) = 0;
        virtual void      WaitIdle() = 0;
        virtual QueueType GetType() const = 0;
    };

} // namespace sky::aurora
