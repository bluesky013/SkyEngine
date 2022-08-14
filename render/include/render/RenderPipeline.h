//
// Created by Zach Lee on 2021/11/14.
//


#pragma once
#include <memory>
#include <vulkan/Semaphore.h>
#include <vulkan/CommandBuffer.h>
#include <vulkan/CommandPool.h>
#include <render/framegraph/FrameGraphEncoder.h>

namespace sky {

    class FrameGraph;
    class RenderScene;
    class RenderViewport;

    class RenderPipeline  {
    public:
        RenderPipeline(RenderScene& scn) : scene(scn) {}
        virtual ~RenderPipeline() = default;

        virtual void BeginFrame(FrameGraph& frameGraph)
        {
            encoders.clear();
        }

        virtual void DoFrame(FrameGraph& frameGraph);

        virtual void EndFrame() {}

        void Setup(RenderViewport& vp);

        virtual void ViewportChange(RenderViewport& vp) {}

    protected:
        RenderScene& scene;
        RenderViewport* viewport = nullptr;

        drv::SemaphorePtr imageAvailable;
        drv::SemaphorePtr renderFinish;
        drv::CommandPoolPtr commandPool;
        drv::CommandBufferPtr commandBuffer;
        drv::Queue* graphicsQueue;

        std::vector<FrameGraphRasterEncoder*> encoders;
    };
    using RDPipeline = std::unique_ptr<RenderPipeline>;

}