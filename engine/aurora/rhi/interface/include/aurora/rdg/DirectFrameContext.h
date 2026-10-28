//
// Created on 2026/10/27.
//

#pragma once

#include <aurora/rdg/FrameExecutionTypes.h>

namespace sky::aurora {

    class Fence;
    using DirectFrameBeginResult = std::variant<FrameError, FrameBusy, FrameOperationComplete>;

    // Device creation selects this protocol explicitly; the caller retires direct submissions before destruction.
    class DirectFrameContext {
    public:
        virtual ~DirectFrameContext()                                 = default;
        virtual DirectFrameBeginResult TryBeginFrame() noexcept       = 0;
        virtual FrameOperationResult   EndFrame() noexcept            = 0;
        virtual Fence                 *GetFrameFence() const noexcept = 0;
    };

} // namespace sky::aurora
