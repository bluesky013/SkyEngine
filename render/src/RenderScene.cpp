//
// Created by Zach Lee on 2021/11/28.
//

#include <core/util/Memory.h>
#include <render/DriverManager.h>
#include <render/Render.h>
#include <render/RenderConstants.h>
#include <render/RenderPipelineForward.h>
#include <render/RenderScene.h>

namespace sky {

    void RenderScene::OnPreRender(float time)
    {
        views.clear();
        for (auto &feature : features) {
            feature.second->OnTick(time);
            feature.second->OnPreparePipeline();
        }

        for (auto &feature : features) {
            feature.second->GatherRenderPrimitives();
        }
    }

    void RenderScene::OnRender(const drv::CommandBufferPtr &commandBuffer)
    {
        if (!pipeline) {
            return;
        }

        FrameGraph frameGraph;
        pipeline->BeginFrame(frameGraph);

        for (auto &feature : features) {
            feature.second->OnRender();
        }

        pipeline->DoFrame(frameGraph, commandBuffer);

        sceneInfo->RequestUpdate();
        mainViewInfo->RequestUpdate();

        for (auto &feature : features) {
            feature.second->OnPostRender();
        }
    }

    void RenderScene::OnPostRender()
    {
        queryPool->ReadResults(1);
    }

    void RenderScene::ViewportChange(RenderViewport &viewport)
    {
        pipeline->ViewportChange(viewport);
        for (auto &feature : features) {
            feature.second->OnViewportSizeChange(viewport);
        }
    }

    void RenderScene::BindViewport(RenderViewport &viewport)
    {
        pipeline = std::make_unique<RenderPipelineForward>(*this);
        for (auto &feature : features) {
            feature.second->OnBindViewport(viewport);
        }
        ViewportChange(viewport);
    }

    void RenderScene::UpdateOutput(const drv::ImagePtr &output)
    {
        if (pipeline) {
            pipeline->SetOutput(output);
        }
    }

    void RenderScene::AddView(const RDViewPtr &view)
    {
        view->Reset();
        views.emplace_back(view);
    }

    const std::vector<RDViewPtr> &RenderScene::GetViews() const
    {
        return views;
    }

    void RenderScene::FillSetBinder(drv::DescriptorSetBinder &binder)
    {
        binder.BindSet(0, sceneSet->GetRHISet());
        binder.SetOffset(0, 0, mainViewInfo->GetDynamicOffset());
        binder.SetOffset(0, 1, sceneInfo->GetDynamicOffset());
    }

    void RenderScene::Setup()
    {
        InitSceneResource();
    }

    void RenderScene::InitSceneResource()
    {
        auto globalPool = Render::Get()->GetGlobalSetPool();
        sceneSet        = globalPool->Allocate();

        auto &deviceProperties = DriverManager::Get()->GetDevice()->GetProperties();

        Buffer::Descriptor bufferDesc      = {};
        uint32_t           sceneInfoStride = Align(sizeof(SceneInfo), static_cast<uint32_t>(deviceProperties.limits.minUniformBufferOffsetAlignment));
        bufferDesc.size                    = sceneInfoStride * INFLIGHT_FRAME;
        bufferDesc.memory                  = VMA_MEMORY_USAGE_CPU_TO_GPU;
        bufferDesc.usage                   = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        bufferDesc.allocCPU                = true;
        auto sceneBuffer                   = std::make_shared<Buffer>(bufferDesc);
        sceneBuffer->InitRHI();
        sceneInfo = std::make_shared<DynamicBufferView>(sceneBuffer, sizeof(SceneInfo), 0, INFLIGHT_FRAME, sceneInfoStride);

        uint32_t viewInfoSize = Align(sizeof(ViewInfo), static_cast<uint32_t>(deviceProperties.limits.minUniformBufferOffsetAlignment));
        bufferDesc.size       = viewInfoSize * INFLIGHT_FRAME;
        auto viewBuffer       = std::make_shared<Buffer>(bufferDesc);
        viewBuffer->InitRHI();
        mainViewInfo = std::make_shared<DynamicBufferView>(viewBuffer, sizeof(ViewInfo), 0, INFLIGHT_FRAME, viewInfoSize);

        sceneSet->UpdateBuffer(0, mainViewInfo);
        sceneSet->UpdateBuffer(1, sceneInfo);
        sceneSet->Update();

        drv::DescriptorSetLayout::Descriptor objSetLayoutInfo = {};
        objSetLayoutInfo.bindings.emplace(
            0, drv::DescriptorSetLayout::SetBinding{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT});
        auto objSetLayout = DriverManager::Get()->GetDevice()->CreateDeviceObject<drv::DescriptorSetLayout>(objSetLayoutInfo);
        objectPool.reset(DescriptorPool::CreatePool(objSetLayout, {DEFAULT_OBJECT_SET_NUM}));

        uint32_t objectInfoSize = Align(sizeof(ObjectInfo), static_cast<uint32_t>(deviceProperties.limits.minUniformBufferOffsetAlignment));
        RenderBufferPool::Descriptor bufferPoolInfo = {};
        bufferPoolInfo.count                        = DEFAULT_OBJECT_BLOCK_NUM;
        bufferPoolInfo.stride                       = objectInfoSize;
        bufferPoolInfo.frame                        = INFLIGHT_FRAME;
        bufferPoolInfo.usage                        = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        bufferPoolInfo.memory                       = VMA_MEMORY_USAGE_CPU_TO_GPU;

        objectBufferPool = std::make_unique<RenderBufferPool>(bufferPoolInfo);

        if (!queryPool) {
            drv::QueryPool::Descriptor queryDesc = {};
            queryDesc.queryType                  = VK_QUERY_TYPE_PIPELINE_STATISTICS;
            queryDesc.queryCount                 = 7;
            queryDesc.pipelineStatistics =
                VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT | VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT |
                VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT | VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT |
                VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT | VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT |
                VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT;
            queryPool = DriverManager::Get()->GetDevice()->CreateDeviceObject<drv::QueryPool>(queryDesc);
        } else {
            queryPool->Reset();
        }
    }

    const RDDynBufferViewPtr &RenderScene::GetSceneBuffer() const
    {
        return sceneInfo;
    }

    const RDDynBufferViewPtr &RenderScene::GetMainViewBuffer() const
    {
        return mainViewInfo;
    }

    DescriptorPool *RenderScene::GetObjectSetPool() const
    {
        return objectPool.get();
    }

    RenderBufferPool *RenderScene::GetObjectBufferPool() const
    {
        return objectBufferPool.get();
    }

    const RDDesGroupPtr &RenderScene::GetSceneSet() const
    {
        return sceneSet;
    }

    const drv::QueryPoolPtr &RenderScene::GetQueryPool() const
    {
        return queryPool;
    }
} // namespace sky