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

        virtual void DoFrame(FrameGraph& frameGraph, const drv::CommandBufferPtr& cmdBuffer);

        virtual void ViewportChange(const RenderViewport& viewport) {}

        virtual void SetOutput(const drv::ImagePtr& output) {}

    protected:
        RenderScene& scene;
        std::vector<FrameGraphRasterEncoder*> encoders;
    };
    using RDPipeline = std::unique_ptr<RenderPipeline>;

}