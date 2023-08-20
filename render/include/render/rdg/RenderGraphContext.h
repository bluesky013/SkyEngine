//
// Created by Zach Lee on 2023/4/16.
//

#pragma once

#include <memory>
#include <core/std/Container.h>
#include <rhi/CommandBuffer.h>
#include <render/rdg/TransientPool.h>
#include <render/RenderQueue.h>

namespace sky::rhi {
    class Device;
}

namespace sky::rdg {

    struct SemaphorePool {
        const rhi::SemaphorePtr &Acquire();
        void Reset();

        uint32_t index = 0;
        std::vector<rhi::SemaphorePtr> imageAvailableSemaList;
    };

    struct RenderGraphContext {
        PmrUnSyncPoolRes resources;
        std::unique_ptr<TransientPool> pool;
        std::unordered_map<std::string, RenderQueuePtr> renderQueues;

        rhi::Device *device = nullptr;
        rhi::Queue *graphicsQueue = nullptr;
        rhi::Queue *transferQueue = nullptr;

        rhi::FencePtr fence;
        rhi::SemaphorePtr renderFinish;
        SemaphorePool imageAvailableSemaPool;
        rhi::CommandBufferPtr mainCommandBuffer;
    };

} // namespace sky::rdg