//
// Created by Zach Lee on 2021/11/28.
//


#include <render/RenderScene.h>
#include <render/DriverManager.h>
#include <render/DevObjManager.h>

namespace sky {

    void RenderScene::OnPreRender()
    {
    }

    void RenderScene::OnPostRender()
    {
    }

    void RenderScene::AddView(RDViewPtr view)
    {
        views.emplace_back(view);
    }

    void RenderScene::RemoveView(RDViewPtr view)
    {
        auto iter = std::find_if(views.begin(), views.end(), [&view](RDViewPtr& rhs) {
            return view.get() == rhs.get();
        });
        if (iter != views.end()) {
            views.erase(iter);
        }
    }

    const std::vector<RDViewPtr>& RenderScene::GetViews() const
    {
        return views;
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