//
// Created by Zach Lee on 2025/9/29.
//

#include "framework/window/NativeWindow.h"
#include "shader/ShaderCompilerGlsl.h"
#include <ImageTool.h>
#include <builder/render/image/ImageLoader.h>
#include <builder/render/image/StbImageLoader.h>
#include <builder/render/image/ImageCompressor.h>
namespace sky {

    std::unique_ptr<ShaderCompilerGlsl> GCompiler;

    void ImageTool::Init(NativeWindow* window)
    {
        wndEvent.Bind(this, window);
        dropEvent.Bind(this);

        mainViewport = window;

        rhi::Instance::Descriptor drvDes = {};
        drvDes.engineName         = "SkyEngine";
        drvDes.appName            = "ImageTool";
        drvDes.api                = rhi::API::VULKAN;
#ifdef _DEBUG
        drvDes.enableDebugLayer   = true;
#endif
        RHI::Get()->InitInstance(drvDes);
        RHI::Get()->InitDevice({});

        device = RHI::Get()->GetDevice();

        rhi::SwapChain::Descriptor desc = {};
        desc.window = window->GetNativeHandle();
        desc.width = window->GetWidth();
        desc.height = window->GetHeight();
        desc.preferredMode = rhi::PresentMode::VSYNC;
        swapChain = RHI::Get()->GetDevice()->CreateSwapChain(desc);

        GCompiler = std::make_unique<ShaderCompilerGlsl>();
        GCompiler->Init();

        InitPipeline();

        builder::ImageLoaderManager::Get()->Register(new builder::StbImageLoader());
        builder::InitializeCompressor();

        imageRender = std::make_unique<ImageRender>(device, window, renderPass);
        gui = std::make_unique<GuiRender>(device, window, renderPass);
        gui->AddWidget(imageRender.get());
    }

    void ImageTool::InitPipeline()
    {
        rhi::RenderPass::SubPass subPass = {};
        subPass.colors.emplace_back(rhi::RenderPass::AttachmentRef{.index=0});

        rhi::RenderPass::Descriptor renderPassDesc = {};
        renderPassDesc.attachments.emplace_back(rhi::RenderPass::Attachment{.format=swapChain->GetFormat(), .sample=rhi::SampleCount::X1, .load=rhi::LoadOp::CLEAR, .store=rhi::StoreOp::STORE});
        renderPassDesc.subPasses.emplace_back(subPass);

        renderPass = device->CreateRenderPass(renderPassDesc);
        commandBuffer = device->CreateCommandBuffer({});
        fence = device->CreateFence({});

        rhi::FrameBuffer::Descriptor frameBufferDesc = {.extent=swapChain->GetExtent(), .pass=renderPass };
        frameBufferDesc.views.resize(1);

        frameBuffers.resize(swapChain->GetImageCount());
        for (uint32_t i = 0; i < swapChain->GetImageCount(); ++i) {
            frameBufferDesc.views[0] = swapChain->GetImageView(i);
            frameBuffers[i] = device->CreateFrameBuffer(frameBufferDesc);
        }

        presentSema = device->CreateSema({});
        for (uint32_t i = 0; i < swapChain->GetImageCount(); ++i) {
            renderSema.emplace_back(device->CreateSema({}));
        }
    }

    void ImageTool::Shutdown()
    {
        device->WaitIdle();
        gui = nullptr;
        imageRender = nullptr;
        swapChain = nullptr;
        mainViewport = nullptr;
        frameBuffers.clear();
        renderPass = nullptr;
        commandBuffer = nullptr;
        fence = nullptr;
        presentSema = nullptr;
        renderSema.clear();

        GCompiler = nullptr;
        RHI::Destroy();
    }

    void ImageTool::Render()
    {
        uint32_t index = swapChain->AcquireNextImage(presentSema);
        commandBuffer->Begin();

        rhi::ImageSubRange range = {.baseLevel=0, .levels=1, .baseLayer=0, .layers=1, .aspectMask=rhi::AspectFlagBit::COLOR_BIT};
        rhi::BarrierInfo barrierInfo = {.srcFlags=rhi::AccessFlagBit::NONE, .dstFlags=rhi::AccessFlagBit::COLOR_WRITE };

        commandBuffer->QueueBarrier(swapChain->GetImage(index), range, barrierInfo);
        commandBuffer->FlushBarriers();

        rhi::ClearValue clear = {0.2f, 0.2f, 0.2f, 1.f};

        auto gfx = commandBuffer->EncodeGraphics();
        gfx->BeginPass(rhi::PassBeginInfo{.frameBuffer=frameBuffers[index], .renderPass=renderPass, .clearCount=1, .clearValues=&clear});

        imageRender->Render(*gfx);
        gui->Render(*gfx);

        gfx->EndPass();

        barrierInfo = {.srcFlags=rhi::AccessFlagBit::COLOR_WRITE, .dstFlags=rhi::AccessFlagBit::PRESENT };
        commandBuffer->QueueBarrier(swapChain->GetImage(index), range, barrierInfo);
        commandBuffer->FlushBarriers();
        commandBuffer->End();

        rhi::SubmitInfo submit = {};
        submit.fence = fence;
        submit.submitSignals.emplace_back(renderSema[currentFrame]);
        submit.waits.emplace_back(rhi::PipelineStageBit::COLOR_OUTPUT, presentSema);

        commandBuffer->Submit(*device->GetQueue(rhi::QueueType::GRAPHICS), submit);

        rhi::PresentInfo presentInfo = {};
        presentInfo.imageIndex = index;
        presentInfo.semaphores.emplace_back(renderSema[currentFrame]);
        swapChain->Present(*device->GetQueue(rhi::QueueType::GRAPHICS), presentInfo);

        currentFrame = (currentFrame + 1) % swapChain->GetImageCount();;
    }

    void ImageTool::Tick(float delta)
    {
        fence->WaitAndReset();

        gui->Tick(delta);
        Render();
    }

    void ImageTool::OnWindowResize(const WindowResizeEvent& event)
    {
        device->WaitIdle();

        swapChain->Resize(event.width, event.height, mainViewport->GetNativeHandle());

        rhi::FrameBuffer::Descriptor frameBufferDesc = {.extent=swapChain->GetExtent(), .pass=renderPass };
        frameBufferDesc.views.resize(1);

        frameBuffers.clear();
        frameBuffers.resize(swapChain->GetImageCount());
        for (uint32_t i = 0; i < swapChain->GetImageCount(); ++i) {
            frameBufferDesc.views[0] = swapChain->GetImageView(i);
            frameBuffers[i] = device->CreateFrameBuffer(frameBufferDesc);
        }
    }

    void ImageTool::OnDrop(const std::string& payload)
    {
        if (imageRender) {
            imageRender->OnDropFile(payload);
        }
    }

} // namespace sky