//
// Created by Zach Lee on 2021/11/28.
//


#include <render/RenderScene.h>
#include <render/DriverManager.h>
#include <render/DevObjManager.h>

namespace sky {

    void RenderScene::OnPreRender()
    {
        views.clear();
        for (auto& feature : features) {
            feature->OnPrepareView(*this);
            feature->GatherRenderItem(*this);
        }
    }

    void RenderScene::OnPostRender()
    {
        for (auto& feature : features) {
            feature->OnPostRender(*this);
        }
    }

    void RenderScene::OnRender()
    {
        for (auto& feature : features) {
            feature->OnRender(*this);
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

    void RenderScene::RegisterFeature(RenderFeature* feature)
    {
        features.emplace_back(std::unique_ptr<RenderFeature>(feature));
    }

//    void RenderScene::OnPostTick()
//    {
//        renderGraph.Compile();
//        auto device = DriverManager::Get()->GetDevice();
//        auto queue = device->GetQueue({VK_QUEUE_GRAPHICS_BIT});
//
//        drv::SemaphorePtr signal = device->CreateDeviceObject<drv::Semaphore>({});
//
//        if (!cmdBuffer) {
//            cmdBuffer = queue->AllocateCommandBuffer(drv::CommandBuffer::Descriptor{});
//        }
//        cmdBuffer->Wait();
//        cmdBuffer->Begin();
//
//        renderGraph.Execute(*cmdBuffer);
//
//        cmdBuffer->End();
//
//        drv::CommandBuffer::SubmitInfo submit = {
//            {{VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, waitSemaphore}},
//            {signal}
//        };
//        cmdBuffer->Submit(*queue, submit);
//
//        drv::SwapChain::PresentInfo present = {
//            {signal}
//        };
//        swapChain->Present(present);
//        DevObjManager::Get()->FreeDeviceObject(signal);
//    }
}