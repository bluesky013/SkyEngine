//
// Created by Zach Lee on 2022/6/16.
//


#include <FrameGraphSample.h>
#include <render/framegraph/FrameGraph.h>
#include <render/framegraph/FrameGraphPass.h>
#include <render/DriverManager.h>

namespace sky::render {

    void FrameGraphSample::Init()
    {
        StartInfo info = {};
        info.appName = "FrameGraphSample";
        Render::Get()->Init(info);

        device = DriverManager::Get()->GetDevice();
    }

    void FrameGraphSample::Start()
    {
        viewport = std::make_unique<RenderViewport>();
        auto nativeWindow = Interface<ISystemNotify>::Get()->GetApi()->GetViewport();
        RenderViewport::ViewportInfo info = {};
        info.wHandle = nativeWindow->GetNativeHandle();
        viewport->Setup(info);

        graphicsQueue = device->GetQueue(VK_QUEUE_GRAPHICS_BIT);
        drv::CommandPool::Descriptor cmdPoolDesc = {};
        cmdPoolDesc.flag = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        cmdPoolDesc.queueFamilyIndex = graphicsQueue->GetQueueFamilyIndex();

        commandPool = device->CreateDeviceObject<drv::CommandPool>(cmdPoolDesc);

        drv::CommandBuffer::Descriptor cmdDesc = {};
        commandBuffer = commandPool->Allocate(cmdDesc);

        Event<IWindowEvent>::Connect(info.wHandle, this);

        auto swapChain = viewport->GetSwapChain();
        auto& ext = swapChain->GetExtent();

        drv::Image::Descriptor dsDesc = {};
        dsDesc.format = VK_FORMAT_D32_SFLOAT;
        dsDesc.extent.width = ext.width;
        dsDesc.extent.height = ext.height;
        dsDesc.samples = VK_SAMPLE_COUNT_4_BIT;
        dsDesc.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        dsDesc.memory = VMA_MEMORY_USAGE_GPU_ONLY;
        depthStencil = device->CreateDeviceObject<drv::Image>(dsDesc);

        dsDesc.format = swapChain->GetFormat();
        dsDesc.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        dsDesc.memory = VMA_MEMORY_USAGE_GPU_ONLY;
        msaaColor = device->CreateDeviceObject<drv::Image>(dsDesc);

        imageAvailable = device->CreateDeviceObject<drv::Semaphore>({});
        renderFinish = device->CreateDeviceObject<drv::Semaphore>({});

        dsDesc.format = VK_FORMAT_D32_SFLOAT;
        dsDesc.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        dsDesc.extent.width = 1024;
        dsDesc.extent.height = 1024;
        dsDesc.samples = VK_SAMPLE_COUNT_1_BIT;
        shadowMap = device->CreateDeviceObject<drv::Image>(dsDesc);

        uint32_t imageIndex = 0;
        swapChain->AcquireNext(imageAvailable, imageIndex);

        graph.AddPass<FrameGraphGraphicPass>("ShadowPass", [&](FrameGraphBuilder& builder) {
            builder.ImportImage("ShadowMapImage", shadowMap);
            builder.CreateImageAttachment("ShadowMapImage", "ShadowMap", VK_IMAGE_ASPECT_DEPTH_BIT);
            builder.WriteAttachment("ShadowMap", ImageBindFlag::DEPTH_STENCIL);
        });

        graph.AddPass<FrameGraphGraphicPass>("ColorPass", [&](FrameGraphBuilder& builder) {
            builder.ImportImage("ColorMSAAImage", msaaColor);
            builder.ImportImage("SwapChainImage", swapChain->GetImage(imageIndex));
            builder.ImportImage("DepthImage", depthStencil);
            builder.CreateImageAttachment("SwapChainImage", "SwapChainOutput", VK_IMAGE_ASPECT_COLOR_BIT);
            builder.CreateImageAttachment("ColorMSAAImage", "ColorMSAA", VK_IMAGE_ASPECT_COLOR_BIT);
            builder.CreateImageAttachment("DepthImage", "DepthOutput", VK_IMAGE_ASPECT_DEPTH_BIT);

            builder.ReadAttachment("ShadowMap", ImageBindFlag::DEPTH_STENCIL_READ);
            builder.WriteAttachment("SwapChainOutput", ImageBindFlag::COLOR_RESOLVE);
            builder.WriteAttachment("ColorMSAA", ImageBindFlag::COLOR);
            builder.WriteAttachment("DepthOutput", ImageBindFlag::DEPTH_STENCIL);
        });

        graph.Compile();
        graph.PrintGraph();
    }

    void FrameGraphSample::Stop()
    {
        viewport->Shutdown();
        viewport = nullptr;

        Render::Get()->Destroy();
    }

    void FrameGraphSample::Tick(float delta)
    {
        uint32_t imageIndex = 0;
        auto swapChain = viewport->GetSwapChain();
        swapChain->AcquireNext(imageAvailable, imageIndex);

        commandBuffer->Wait();
        commandBuffer->Begin();

        graph.Execute(commandBuffer);

        drv::CommandBuffer::SubmitInfo submitInfo = {};
        submitInfo.submitSignals.emplace_back(renderFinish);
        submitInfo.waits.emplace_back(std::pair<VkPipelineStageFlags, drv::SemaphorePtr>{
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, imageAvailable
        });

        commandBuffer->Submit(*graphicsQueue, submitInfo);

        drv::SwapChain::PresentInfo presentInfo = {};
        presentInfo.imageIndex = imageIndex;
        presentInfo.signals.emplace_back(renderFinish);
        swapChain->Present(presentInfo);
    }
}

REGISTER_MODULE(sky::render::FrameGraphSample)