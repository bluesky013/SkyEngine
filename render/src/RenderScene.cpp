//
// Created by Zach Lee on 2021/11/28.
//


#include <render/RenderScene.h>
#include <render/DriverManager.h>
#include <render/DevObjManager.h>
#include <render/RenderPipelineForward.h>
#include <render/Render.h>

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

        InitSceneResource();
    }

    void RenderScene::ViewportChange(RenderViewport& viewport)
    {
        pipeline->ViewportChange(viewport);
    }

    void RenderScene::InitSceneResource()
    {
        auto globalPool = Render::Get()->GetGlobalSetPool();
        sceneSet = globalPool->Allocate();

        Buffer::Descriptor bufferDesc = {};
        bufferDesc.size = sizeof(SceneInfo);
        bufferDesc.memory = VMA_MEMORY_USAGE_CPU_TO_GPU;
        bufferDesc.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        bufferDesc.keepCPU = true;
        auto sceneBuffer = std::make_shared<Buffer>(bufferDesc);
        sceneBuffer->InitRHI();
        sceneInfo = std::make_shared<BufferView>(sceneBuffer, sizeof(SceneInfo), 0);

        bufferDesc.size = sizeof(ViewInfo);
        auto viewBuffer = std::make_shared<Buffer>(bufferDesc);
        viewBuffer->InitRHI();
        mainViewInfo = std::make_shared<BufferView>(viewBuffer, sizeof(ViewInfo), 0);

        sceneSet->UpdateBuffer(0, mainViewInfo);
        sceneSet->UpdateBuffer(1, sceneInfo);
        sceneSet->Update();
    }

    RDBufferViewPtr RenderScene::GetSceneBuffer() const
    {
        return sceneInfo;
    }

    RDBufferViewPtr RenderScene::GetMainViewBuffer() const
    {
        return mainViewInfo;
    }
}