//
// Created by Zach Lee on 2021/11/14.
//


#pragma once
#include <memory>
#include <vulkan/Semaphore.h>
#include <vulkan/CommandBuffer.h>
#include <vulkan/CommandPool.h>

namespace sky {

    class RenderScene;
    class RenderViewport;

    class RenderPipeline  {
    public:
        RenderPipeline() = default;
        virtual ~RenderPipeline() = default;

        virtual void BeginFrame() {}

        virtual void DoFrame() {}

        virtual void EndFrame() {}

        void Setup(RenderViewport& vp);

        virtual void ViewportChange(RenderViewport& vp) {}

    protected:
        RenderViewport* viewport = nullptr;

        drv::SemaphorePtr imageAvailable;
        drv::SemaphorePtr renderFinish;
        drv::CommandPoolPtr commandPool;
        drv::CommandBufferPtr commandBuffer;
        drv::Queue* graphicsQueue;
    };
    using RDPipeline = std::shared_ptr<RenderPipeline>;

}