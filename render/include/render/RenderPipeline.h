//
// Created by Zach Lee on 2021/11/14.
//


#pragma once
#include <memory>
#include <vulkan/Semaphore.h>
#include <vulkan/CommandBuffer.h>
#include <vulkan/CommandPool.h>
#include <render/RenderEncoder.h>
#include <render/RenderConstants.h>

namespace sky {

    class FrameGraph;
    class RenderScene;
    class RenderViewport;

    static constexpr uint32_t FORWARD_TAG = 0x01;
    static constexpr uint32_t UI_TAG      = 0x02;

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
        std::vector<RenderRasterEncoder*> encoders;
    };
    using RDPipeline = std::unique_ptr<RenderPipeline>;

}