
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
        RenderScene(Render& rd) : render(rd) {}
        ~RenderScene();

        using PipelienPtr = std::unique_ptr<RenderPipeline>;

        void SetTarget(drv::SwapChainPtr& swc);

        void SetRenderPipeline(PipelienPtr&& ptr);

        void AddView(RenderView* view);

        void OnPreTick();

        void OnTick(float time);

        void OnPostTick();
    private:
        Render& render;
        drv::SwapChainPtr swapChain;
        PipelienPtr pipeline;
        RenderGraph renderGraph;
        std::list<RenderView*> views;
    };

}