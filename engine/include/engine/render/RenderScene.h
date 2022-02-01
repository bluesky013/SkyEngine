
//
// Created by Zach Lee on 2021/11/14.
//


#pragma once

#include <engine/world/World.h>
#include <vulkan/Swapchain.h>
#include <engine/render/RenderPipeline.h>
#include <engine/render/RenderView.h>
#include <engine/render/rendergraph/RenderGraph.h>

namespace sky {

    class Render;
    class RenderPipeline;

    class RenderScene {
    public:
        RenderScene() = default;
        ~RenderScene() = default;

        using PipelinePtr = std::unique_ptr<RenderPipeline>;

        void SetTarget(drv::SwapChainPtr& swc);

        void SetRenderPipeline(PipelinePtr&& ptr);

        void AddView(RenderView* view);

        void OnPreTick();

        void OnTick(float time);

        void OnPostTick();
    private:
        drv::SwapChainPtr swapChain;
        PipelinePtr pipeline;
        RenderGraph renderGraph;
        std::list<RenderView*> views;
        drv::SemaphorePtr waitSemaphore;
        drv::CommandBufferPtr cmdBuffer;
    };

}