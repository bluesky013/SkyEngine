//
// Created by Zach Lee on 2023/4/16.
//

#pragma once

#include <memory>
#include <core/std/Container.h>
#include <rhi/CommandBuffer.h>
#include <render/rdg/TransientPool.h>
#include <render/rdg/RenderGraphTypes.h>
#include <render/resource/ResourceGroup.h>

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

        rhi::Device *device = nullptr;
        rhi::Queue *graphicsQueue = nullptr;
        rhi::Queue *transferQueue = nullptr;

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

} // namespace sky::rdg
