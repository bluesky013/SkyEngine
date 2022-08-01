//
// Created by Zach Lee on 2022/7/18.
//

#include <render/RenderPipelineForward.h>
#include <render/framegraph/FrameGraphPass.h>
#include <render/framegraph/FrameGraphBuilder.h>
#include <render/RenderViewport.h>
#include <render/DriverManager.h>
#include <vulkan/Util.h>

namespace sky {

    void RenderPipelineForward::ViewportChange(RenderViewport& vp)
    {
        auto swapChain = viewport->GetSwapChain();
        auto& ext = swapChain->GetExtent();
        auto device = DriverManager::Get()->GetDevice();

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
    }

    void RenderPipelineForward::BeginFrame()
    {
        RenderPipeline::BeginFrame();
        auto swapChain = viewport->GetSwapChain();
        swapChain->AcquireNext(imageAvailable, currentImageIndex);

        auto clearColor = drv::MakeClearColor(0.f, 0.f, 0.f, 0.f);
        auto clearDS = drv::MakeClearDepthStencil(1.f, 0);

        currentFrame = std::make_unique<FrameGraph>();
        currentFrame->AddPass<FrameGraphEmptyPass>("preparePass", [&](FrameGraphBuilder& builder) {
            builder.ImportImage("ColorMSAAImage", msaaColor);
            builder.ImportImage("ColorResolveImage", swapChain->GetImage(currentImageIndex));
            builder.ImportImage("DepthImage", depthStencil);
        });

        auto pass = currentFrame->AddPass<FrameGraphGraphicPass>("ColorPass", [&](FrameGraphBuilder& builder) {
            builder.CreateImageAttachment("ColorMSAAImage", "ColorMSAA", VK_IMAGE_ASPECT_COLOR_BIT)
                ->SetColorOp(VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE)
                .SetClearValue(clearColor);

            builder.CreateImageAttachment("ColorResolveImage", "ColorResolve", VK_IMAGE_ASPECT_COLOR_BIT)
                ->SetColorOp(VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE)
                .SetClearValue(clearColor);

            builder.CreateImageAttachment("DepthImage", "DepthOutput", VK_IMAGE_ASPECT_DEPTH_BIT)
                ->SetColorOp(VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE)
                .SetClearValue(clearDS);

            builder.WriteAttachment("ColorMSAA", ImageBindFlag::COLOR);
            builder.WriteAttachment("ColorResolve", ImageBindFlag::COLOR_RESOLVE);
            builder.WriteAttachment("DepthOutput", ImageBindFlag::DEPTH_STENCIL);
        });
        auto encoder = scene.RegisterEncoder<FrameGraphRasterEncoder>(FORWARD_TAG);
        encoders.emplace_back(encoder);
        pass->SetEncoder(encoder);

        currentFrame->AddPass<FrameGraphEmptyPass>("Present", [&](FrameGraphBuilder& builder) {
            builder.ReadAttachment("ColorResolve", ImageBindFlag::PRESENT);
        });

        currentFrame->Compile();

        commandBuffer->Wait();
    }

    void RenderPipelineForward::DoFrame()
    {
        commandBuffer->Begin();
        currentFrame->Execute(commandBuffer);

        commandBuffer->End();
        lastFrame.reset(currentFrame.release());
    }

    void RenderPipelineForward::EndFrame()
    {
        drv::CommandBuffer::SubmitInfo submitInfo = {};
        submitInfo.submitSignals.emplace_back(renderFinish);
        submitInfo.waits.emplace_back(std::pair<VkPipelineStageFlags, drv::SemaphorePtr>{
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, imageAvailable
        });
        commandBuffer->Submit(*graphicsQueue, submitInfo);

        drv::SwapChain::PresentInfo presentInfo = {};
        presentInfo.imageIndex = currentImageIndex;
        presentInfo.signals.emplace_back(renderFinish);

        auto swapChain = viewport->GetSwapChain();
        swapChain->Present(presentInfo);
    }
}