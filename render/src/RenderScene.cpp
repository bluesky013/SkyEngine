//
// Created by Zach Lee on 2021/11/28.
//


#include <render/RenderScene.h>
#include <render/DriverManager.h>
#include <render/DevObjManager.h>
#include <render/RenderPipelineForward.h>

namespace sky {

    void RenderScene::OnPreRender()
    {
        views.clear();
        for (auto& feature : features) {
            feature.second->OnPrepareView(*this);
        }
        if (pipeline) {
            pipeline->BeginFrame();
        }
    }

    void RenderScene::OnPostRender()
    {
        for (auto& feature : features) {
            feature.second->OnPostRender(*this);
        }

        if (pipeline) {
            pipeline->EndFrame();
        }
    }

    void RenderScene::OnRender()
    {
        for (auto& feature : features) {
            feature.second->GatherRenderItem(*this);
            feature.second->OnRender(*this);
        }

        if (pipeline) {
            pipeline->DoFrame();
        }
    }

    void RenderScene::AddView(RDViewPtr view)
    {
        views.emplace_back(view);
    }

    const std::vector<RDViewPtr>& RenderScene::GetViews() const
    {
        return views;
    }

    void RenderScene::Setup(RenderViewport& viewport)
    {
        pipeline = std::make_unique<RenderPipelineForward>();
        pipeline->Setup(viewport);
        ViewportChange(viewport);
    }

    void RenderScene::ViewportChange(RenderViewport& viewport)
    {
        pipeline->ViewportChange(viewport);
    }
}