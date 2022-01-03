//
// Created by Zach Lee on 2021/11/28.
//


#include <engine/render/RenderScene.h>
#include <engine/render/DriverManager.h>
#include <vulkan/CommandBuffer.h>
#include <vulkan/CommandPool.h>

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
        renderGraph.Compile();
        auto device = DriverManager::Get()->GetDevice();
        auto queue = device->GetQueue({VK_QUEUE_GRAPHICS_BIT});

        drv::SemaphorePtr wait = device->CreateDeviceObject<drv::Semaphore>({});
        drv::SemaphorePtr signal = device->CreateDeviceObject<drv::Semaphore>({});

        swapChain->AcquireNext(wait);

        auto cmd = queue->AllocateCommandBuffer(drv::CommandBuffer::Descriptor{});
        cmd->Begin();

        renderGraph.Execute(*cmd);

        cmd->End();

        drv::CommandBuffer::SubmitInfo submit = {
            {{VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, wait}},
            {signal}
        };
        cmd->Submit(*queue, submit);

        drv::SwapChain::PresentInfo present = {
            {signal}
        };
        swapChain->Present(present);
    }
}