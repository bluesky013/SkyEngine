//
// Created by Zach Lee on 2022/6/21.
//

#include <render/RHIManager.h>
#include <render/RenderViewport.h>

namespace sky {

    RenderViewport::~RenderViewport()
    {
        Shutdown();
    }

    void RenderViewport::Setup(const ViewportInfo &info)
    {
        vk::SwapChain::VkDescriptor descriptor = {};
        descriptor.window                     = info.wHandle;
        descriptor.preferredMode              = VK_PRESENT_MODE_FIFO_KHR;

        auto device  = RHIManager::Get()->GetDevice();
        swapChain    = device->CreateDeviceObject<vk::SwapChain>(descriptor);
        nativeHandle = info.wHandle;
        Event<IWindowEvent>::Connect(descriptor.window, this);

        graphicsQueue = device->GetGraphicsQueue();
        vk::CommandBuffer::Descriptor cmdDesc = {};
        for (uint32_t i = 0; i < INFLIGHT_FRAME; ++i) {
            commandBuffer[i]  = graphicsQueue->AllocateCommandBuffer(cmdDesc);
            imageAvailable[i] = device->CreateDeviceObject<vk::Semaphore>({});
            renderFinish[i]   = device->CreateDeviceObject<vk::Semaphore>({});
        }
    }

    void RenderViewport::SetScene(const RDScenePtr &scn)
    {
        scene = scn;
        scene->BindViewport(*this);
    }

    void RenderViewport::Shutdown()
    {
        if (swapChain != nullptr) {
            auto device = RHIManager::Get()->GetDevice();
            device->WaitIdle();
            swapChain = nullptr;
        }

        for (uint32_t i = 0; i < INFLIGHT_FRAME; ++i) {
            imageAvailable[i] = nullptr;
            renderFinish[i]   = nullptr;
            commandBuffer[i]  = nullptr;
        }
        graphicsQueue = nullptr;
        scene         = nullptr;
        Event<IWindowEvent>::DisConnect(this);
    }

    vk::SwapChainPtr RenderViewport::GetSwapChain() const
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

    const VkExtent2D &RenderViewport::GetExtent() const
    {
        return swapChain->GetExtent();
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

        vk::CommandBuffer::SubmitInfo submitInfo = {};
        submitInfo.submitSignals.emplace_back(renderFinish[currentFrame]);
        submitInfo.waits.emplace_back(
            std::pair<VkPipelineStageFlags, vk::SemaphorePtr>{VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, imageAvailable[currentFrame]});
        commandBuffer[currentFrame]->Submit(*graphicsQueue, submitInfo);

        vk::SwapChain::PresentInfo presentInfo = {};
        presentInfo.imageIndex                  = currentImageIndex;
        presentInfo.signals.emplace_back(renderFinish[currentFrame]);

        swapChain->Present(presentInfo);

        currentFrame = (currentFrame + 1) % INFLIGHT_FRAME;
    }

    void *RenderViewport::GetNativeHandle() const
    {
        return nativeHandle;
    }

    void RenderViewport::OnWindowResize(uint32_t width, uint32_t height)
    {
        swapChain->Resize(width, height);
        scene->ViewportChange(*this);
    }

} // namespace sky
