//
// Created by Zach Lee on 2021/11/14.
//


#pragma once
#include <memory>
#include <vulkan/Semaphore.h>
#include <vulkan/CommandBuffer.h>
#include <vulkan/CommandPool.h>
#include <render/framegraph/FrameGraphEncoder.h>
#include <render/RenderConstants.h>

namespace sky {

    class FrameGraph;
    class RenderScene;
    class RenderViewport;

    class RenderPipeline  {
    public:
        RenderPipeline(RenderScene& scn) : scene(scn) {}
        virtual ~RenderPipeline();

        virtual void BeginFrame(FrameGraph& frameGraph)
        {
            encoders.clear();
        }

        virtual void DoFrame(FrameGraph& frameGraph);

        virtual void EndFrame()
        {
            currentFrame = (currentFrame + 1) % INFLIGHT_FRAME;
        }

        void Setup(RenderViewport& vp);

        virtual void ViewportChange(RenderViewport& vp) {}

    protected:
        RenderScene& scene;
        RenderViewport* viewport = nullptr;
        uint32_t currentFrame = 0;

        drv::SemaphorePtr imageAvailable[INFLIGHT_FRAME];
        drv::SemaphorePtr renderFinish[INFLIGHT_FRAME];
        drv::CommandBufferPtr commandBuffer[INFLIGHT_FRAME];
        drv::CommandPoolPtr commandPool;
        drv::Queue* graphicsQueue;

        std::vector<FrameGraphRasterEncoder*> encoders;
    };
    using RDPipeline = std::unique_ptr<RenderPipeline>;

}