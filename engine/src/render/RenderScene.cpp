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
        RenderGraphBuilder builder(renderGraph);
        if (pipeline) {
            pipeline->Prepare(builder, views);
        }
    }

    void RenderScene::OnPostTick()
    {

    }
}