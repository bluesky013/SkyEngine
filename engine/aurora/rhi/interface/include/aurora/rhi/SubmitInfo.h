//
// Aurora RHI SubmitInfo.
//

#pragma once

#include <aurora/rhi/Core.h>
#include <vector>

namespace sky::aurora {

    class CommandBuffer;
    class Semaphore;
    class Fence;

    struct SemaphoreSubmitInfo {
        Semaphore         *semaphore = nullptr;
        uint64_t           value     = 0;                          // ignored for binary
        PipelineStageFlags stageMask = PipelineStageBit::TOP;      // wait/signal stage
    };

    struct SubmitInfo {
        std::vector<CommandBuffer*>      commandBuffers;
        std::vector<SemaphoreSubmitInfo> waitSemaphores;
        std::vector<SemaphoreSubmitInfo> signalSemaphores;
        Fence                           *fence = nullptr;
    };

} // namespace sky::aurora
