//
// Created by Zach Lee on 2021/11/28.
//


#include <engine/render/RenderScene.h>
#include <engine/render/DriverManager.h>
#include <engine/render/DevObjManager.h>

namespace sky {

    void RenderScene::SetTarget(drv::SwapChainPtr& swc)
    {
        swapChain = swc;
        pipeline->SetSwapChain(swapChain);
    }

    void RenderScene::SetRenderPipeline(PipelinePtr&& ptr)
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
        auto device = DriverManager::Get()->GetDevice();
        if (!waitSemaphore) {
            waitSemaphore = device->CreateDeviceObject<drv::Semaphore>({});
        }
        swapChain->AcquireNext(waitSemaphore);
    }

    void RenderScene::OnTick(float time)
    {
        pipeline->Render(*this, renderGraph);
    }

    void RenderScene::OnPostTick()
    {
        renderGraph.Compile();
        auto device = DriverManager::Get()->GetDevice();
        auto queue = device->GetQueue({VK_QUEUE_GRAPHICS_BIT});

        drv::SemaphorePtr signal = device->CreateDeviceObject<drv::Semaphore>({});

        if (!cmdBuffer) {
            cmdBuffer = queue->AllocateCommandBuffer(drv::CommandBuffer::Descriptor{});
        }
        cmdBuffer->Wait();
        cmdBuffer->Begin();

        renderGraph.Execute(*cmdBuffer);

        cmdBuffer->End();

        drv::CommandBuffer::SubmitInfo submit = {
            {{VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, waitSemaphore}},
            {signal}
        };
        cmdBuffer->Submit(*queue, submit);

        drv::SwapChain::PresentInfo present = {
            {signal}
        };
        swapChain->Present(present);
        DevObjManager::Get()->FreeDeviceObject(signal);
    }
}