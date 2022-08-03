//
// Created by Zach Lee on 2021/11/28.
//


#include <render/RenderScene.h>
#include <render/DriverManager.h>
#include <render/DevObjManager.h>
#include <render/RenderPipelineForward.h>
#include <render/Render.h>
#include <render/RenderConstants.h>

namespace sky {

    void RenderScene::OnPreRender()
    {
        views.clear();
        for (auto& feature : features) {
            feature.second->OnPreparePipeline();
        }

        for (auto& feature : features) {
            feature.second->GatherRenderPrimitives();
        }

        if (pipeline) {
            pipeline->BeginFrame();
        }
    }

    void RenderScene::OnRender()
    {
        sceneInfo->RequestUpdate();
        mainViewInfo->RequestUpdate();

        for (auto& feature : features) {
            feature.second->OnRender();
        }

        if (pipeline) {
            pipeline->DoFrame();
        }
    }

    void RenderScene::OnPostRender()
    {
        for (auto& feature : features) {
            feature.second->OnPostRender();
        }

        if (pipeline) {
            pipeline->EndFrame();
        }
    }

    void RenderScene::AddView(RDViewPtr view)
    {
        view->Reset();
        views.emplace_back(view);
    }

    const std::vector<RDViewPtr>& RenderScene::GetViews() const
    {
        return views;
    }

    void RenderScene::Setup(RenderViewport& viewport)
    {
        pipeline = std::make_unique<RenderPipelineForward>(*this);
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
        bufferDesc.allocCPU = true;
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

        drv::DescriptorSetLayout::Descriptor objSetLayoutInfo = {};
        objSetLayoutInfo.bindings.emplace(0, drv::DescriptorSetLayout::SetBinding{
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT
        });
        auto objSetLayout = DriverManager::Get()->GetDevice()->CreateDeviceObject<drv::DescriptorSetLayout>(objSetLayoutInfo);
        objectPool = DescriptorPool::CreatePool(objSetLayout, {DEFAULT_OBJECT_SET_NUM});
    }

    RDBufferViewPtr RenderScene::GetSceneBuffer() const
    {
        return sceneInfo;
    }

    RDBufferViewPtr RenderScene::GetMainViewBuffer() const
    {
        return mainViewInfo;
    }

    RDDescriptorPoolPtr RenderScene::GetObjectSetPool() const
    {
        return objectPool;
    }

    RDDesGroupPtr RenderScene::GetSceneSet() const
    {
        return sceneSet;
    }
}