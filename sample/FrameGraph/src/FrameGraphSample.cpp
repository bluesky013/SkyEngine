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
        auto swapChain = viewport->GetSwapChain();
        auto& ext = swapChain->GetExtent();

        drv::Image::Descriptor dsDesc = {};
        dsDesc.format = VK_FORMAT_D32_SFLOAT;
        dsDesc.extent.width = ext.width;
        dsDesc.extent.height = ext.height;
        dsDesc.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        dsDesc.memory = VMA_MEMORY_USAGE_GPU_ONLY;
        depthStencil = device->CreateDeviceObject<drv::Image>(dsDesc);

        imageAvailable = device->CreateDeviceObject<drv::Semaphore>({});
        renderFinish = device->CreateDeviceObject<drv::Semaphore>({});


        uint32_t imageIndex = 0;
        swapChain->AcquireNext(imageAvailable, imageIndex);

        FrameGraph graph;

        graph.AddPass<FrameGraphGraphicPass>("ColorPass", [&](FrameGraphBuilder& builder) {
            builder.ImportImage("SwapChainImage", swapChain->GetImage(imageIndex));
            builder.ImportImage("DepthImage", depthStencil);
            builder.WriteImage("SwapChainImage", "SwapChainOutput", {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL});
            builder.WriteImage("DepthImage", "DepthOutput", {VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL});
        });

        graph.Compile();
        graph.Execute();

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
    }
}

REGISTER_MODULE(sky::render::FrameGraphSample)