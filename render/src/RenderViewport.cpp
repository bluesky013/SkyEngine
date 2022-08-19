//
// Created by Zach Lee on 2022/6/21.
//

#include <render/RenderViewport.h>
#include <render/DriverManager.h>

namespace sky {

    RenderViewport::~RenderViewport()
    {
        Shutdown();
    }

    void RenderViewport::Setup(const ViewportInfo& info)
    {
        drv::SwapChain::Descriptor descriptor = {};
        descriptor.window = info.wHandle;

        auto device = DriverManager::Get()->GetDevice();
        swapChain = device->CreateDeviceObject<drv::SwapChain>(descriptor);
        nativeHandle = info.wHandle;
        Event<IWindowEvent>::Connect(descriptor.window, this);

        graphicsQueue = device->GetQueue(VK_QUEUE_GRAPHICS_BIT);;
        drv::CommandPool::Descriptor cmdPoolDesc = {};
        cmdPoolDesc.flag = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        cmdPoolDesc.queueFamilyIndex = graphicsQueue->GetQueueFamilyIndex();

        commandPool = device->CreateDeviceObject<drv::CommandPool>(cmdPoolDesc);

        drv::CommandBuffer::Descriptor cmdDesc = {};
        for (uint32_t i = 0; i < INFLIGHT_FRAME; ++i) {
            commandBuffer[i] = commandPool->Allocate(cmdDesc);
            imageAvailable[i] = device->CreateDeviceObject<drv::Semaphore>({});
            renderFinish[i] = device->CreateDeviceObject<drv::Semaphore>({});
        }
    }

    void RenderViewport::SetScene(const RDScenePtr& scn)
    {
        scene = scn;
        scene->SetupPipeline(*this);
    }

    void RenderViewport::Shutdown()
    {
        if (swapChain != nullptr) {
            auto device = DriverManager::Get()->GetDevice();
            device->WaitIdle();
            swapChain = nullptr;
        }

        for (uint32_t i = 0; i < INFLIGHT_FRAME; ++i) {
            imageAvailable[i] = nullptr;
            renderFinish[i] = nullptr;
            commandBuffer[i] = nullptr;
        }
        commandPool = nullptr;
        graphicsQueue = nullptr;
        scene = nullptr;
        Event<IWindowEvent>::DisConnect(this);
    }

    drv::SwapChainPtr RenderViewport::GetSwapChain() const
    {
        return swapChain;
    }

    void RenderViewport::DoFrame()
    {
        BeginFrame();

        if (scene) {
            scene->UpdateOutput(swapChain->GetImage(currentImageIndex));
            scene->OnRender(commandBuffer[currentFrame]);
        }

        EndFrame();
    }

    void RenderViewport::BeginFrame()
    {
        swapChain->AcquireNext(imageAvailable[currentFrame], currentImageIndex);

        commandBuffer[currentFrame]->Wait();
        commandBuffer[currentFrame]->Begin();
    }

    void RenderViewport::EndFrame()
    {
        commandBuffer[currentFrame]->End();

        drv::CommandBuffer::SubmitInfo submitInfo = {};
        submitInfo.submitSignals.emplace_back(renderFinish[currentFrame]);
        submitInfo.waits.emplace_back(std::pair<VkPipelineStageFlags, drv::SemaphorePtr>{
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, imageAvailable[currentFrame]
        });
        commandBuffer[currentFrame]->Submit(*graphicsQueue, submitInfo);

        drv::SwapChain::PresentInfo presentInfo = {};
        presentInfo.imageIndex = currentImageIndex;
        presentInfo.signals.emplace_back(renderFinish[currentFrame]);

        swapChain->Present(presentInfo);

        currentFrame = (currentFrame + 1) % INFLIGHT_FRAME;
    }

    void* RenderViewport::GetNativeHandle() const
    {
        return nativeHandle;
    }

    void RenderViewport::OnWindowResize(uint32_t width, uint32_t height)
    {
        swapChain->Resize(width, height);
        scene->ViewportChange(*this);
    }

}