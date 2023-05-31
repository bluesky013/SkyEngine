//
// Created by Zach Lee on 2023/4/16.
//

#pragma once

#include <memory>
#include <core/std/Container.h>
#include <rhi/CommandBuffer.h>
#include <render/rdg/TransientPool.h>
#include <render/RenderQueue.h>

namespace sky::rdg {

    struct RenderGraphContext {
        PmrUnSyncPoolRes resources;
        std::unique_ptr<TransientPool> pool;
        std::unordered_map<std::string, RenderQueuePtr> renderQueues;

        rhi::Queue *graphicsQueue = nullptr;
        rhi::Queue *transferQueue = nullptr;
        rhi::CommandBufferPtr mainCommandBuffer;
    };

} // namespace sky::rdg