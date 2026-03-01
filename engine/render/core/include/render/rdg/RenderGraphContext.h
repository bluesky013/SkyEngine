//
// Created by Zach Lee on 2023/4/16.
//

#pragma once

#include <memory>
#include <core/std/Container.h>
#include <core/memory/LinearStorage.h>
#include <taskflow/taskflow.hpp>
#include <rhi/CommandBuffer.h>
#include <render/rdg/TransientPool.h>
#include <render/rdg/RenderGraphTypes.h>
#include <render/rdg/RenderGraphData.h>
#include <render/resource/ResourceGroup.h>

namespace sky::rhi {
    class Device;
}

namespace sky::rdg {

    static constexpr uint32_t RDG_TRANSIENT_BLOCK_SIZE = 64 * 1024;

    struct SemaphorePool {
        const rhi::SemaphorePtr &Acquire();
        void Reset();

        uint32_t index = 0;
        std::vector<rhi::SemaphorePtr> imageAvailableSemaList;
    };

    struct RenderGraphContext {
        explicit RenderGraphContext(size_t workThreadNum) : executor(workThreadNum)
        {
        }

        tf::Executor executor;
        PmrUnSyncPoolRes resources;
        std::unique_ptr<TransientPool> pool;

        rhi::Device *device = nullptr;
        rhi::Queue *graphicsQueue = nullptr;
        rhi::Queue *transferQueue = nullptr;

        LinearStorage transientStorage { RDG_TRANSIENT_BLOCK_SIZE }; // storage for frame data, pod only.

        RenderGraphData rdgData;

        uint32_t frameIndex = 0;
        RDResourceGroupPtr emptySet;
        std::vector<rhi::FencePtr> fences;
        std::vector<rhi::CommandBufferPtr> commandBuffers;
        std::vector<rhi::SemaphorePtr> renderFinishSemaphores;
        std::vector<SemaphorePool> imageAvailableSemaPools;

        const rhi::FencePtr &Fence() const { return fences[frameIndex]; }
        const rhi::CommandBufferPtr &MainCommandBuffer() const { return commandBuffers[frameIndex]; }
        const rhi::SemaphorePtr &RenderFinishSemaphore() const { return renderFinishSemaphores[frameIndex]; }
        SemaphorePool &ImageAvailableSemaPool() { return imageAvailableSemaPools[frameIndex]; }
    };

    struct RenderGraphTLSContext {
        rhi::CommandBufferPtr commandBuffer;

        LinearStorage transientStorage { RDG_TRANSIENT_BLOCK_SIZE }; // storage for frame data, pod only.
    };

} // namespace sky::rdg
