//
// Created by Zach Lee on 2021/11/28.
//


#include <engine/render/RenderScene.h>

namespace sky {

    RenderScene::~RenderScene()
    {
    }

    void RenderScene::SetTarget(drv::SwapChainPtr& swc)
    {
        swapChain = swc;
        pipeline->SetSwapChain(swapChain);
    }

    void RenderScene::SetRenderPipeline(PipelienPtr&& ptr)
    {
        pipeline.reset(ptr.release());
    }

    void RenderScene::AddView(RenderView* view)
    {
        views.emplace_back(view);
    }

    void RenderScene::OnPreTick()
    {
        views.clear();
        renderGraph.Clear();
    }

    void RenderScene::OnTick(float time)
    {
        pipeline->Render(renderGraph);
    }

    void RenderScene::OnPostTick()
    {
    }
}