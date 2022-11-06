//
// Created by Zach Lee on 2022/7/18.
//

#include <render/RHIManager.h>
#include <render/RenderPipelineForward.h>
#include <render/RenderScene.h>
#include <render/RenderViewport.h>
#include <render/framegraph/FrameGraphBuilder.h>
#include <render/framegraph/FrameGraphPass.h>
#include <vulkan/Util.h>

namespace sky {

    void RenderPipelineForward::ViewportChange(const RenderViewport &viewport)
    {
        auto  swapChain = viewport.GetSwapChain();
        auto &ext       = swapChain->GetExtent();
        auto  device    = RHIManager::Get()->GetDevice();

        vk::Image::Descriptor dsDesc = {};
        dsDesc.format                 = VK_FORMAT_D32_SFLOAT;
        dsDesc.extent.width           = ext.width;
        dsDesc.extent.height          = ext.height;
        dsDesc.samples                = VK_SAMPLE_COUNT_4_BIT;
        dsDesc.usage                  = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        dsDesc.memory                 = VMA_MEMORY_USAGE_GPU_ONLY;
        depthStencil                  = device->CreateDeviceObject<vk::Image>(dsDesc);

        dsDesc.format = swapChain->GetFormat();
        dsDesc.usage  = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        dsDesc.memory = VMA_MEMORY_USAGE_GPU_ONLY;
        msaaColor     = device->CreateDeviceObject<vk::Image>(dsDesc);
    }

    void RenderPipelineForward::SetOutput(const vk::ImagePtr &output)
    {
        colorOut = output;
    }

    void RenderPipelineForward::BeginFrame(FrameGraph &frameGraph)
    {
        RenderPipeline::BeginFrame(frameGraph);
        auto clearColor = vk::MakeClearColor(0.5f, 0.5f, 0.5f, 1.f);
        auto clearDS    = vk::MakeClearDepthStencil(1.f, 0);

        frameGraph.AddPass<FrameGraphEmptyPass>("preparePass", [&](FrameGraphBuilder &builder) {
            builder.ImportImage("ColorMSAAImage", msaaColor);
            builder.ImportImage("ColorResolveImage", colorOut);
            builder.ImportImage("DepthImage", depthStencil);
        });

        auto pass = frameGraph.AddPass<FrameGraphGraphicPass>("ColorPass", [&](FrameGraphBuilder &builder) {
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
        {
            auto encoder = scene.RegisterEncoder<RenderRasterEncoder>(FORWARD_TAG);
            encoders.emplace_back(encoder);
            pass->SetEncoder(encoder);
        }

        auto uiPass = frameGraph.AddPass<FrameGraphGraphicPass>("UIPass", [&](FrameGraphBuilder &builder) {
            builder.ReadWriteAttachment("ColorResolve", "ColorOutput", ImageBindFlag::COLOR)
                ->SetColorOp(VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE);
        });
        {
            auto encoder = scene.RegisterEncoder<RenderRasterEncoder>(UI_TAG);
            encoders.emplace_back(encoder);
            uiPass->SetEncoder(encoder);
        }

        frameGraph.AddPass<FrameGraphEmptyPass>("Present",
                                                [&](FrameGraphBuilder &builder) { builder.ReadAttachment("ColorOutput", ImageBindFlag::PRESENT); });
    }

    void RenderPipelineForward::DoFrame(FrameGraph &frameGraph, const vk::CommandBufferPtr &commandBuffer)
    {
        auto &queryPool = scene.GetQueryPool();

        if (queryPool != nullptr) {
            commandBuffer->ResetQueryPool(queryPool, 0, 1);
            commandBuffer->BeginQuery(queryPool, 0);
        }

        RenderPipeline::DoFrame(frameGraph, commandBuffer);
        frameGraph.Execute(commandBuffer);

        if (queryPool != nullptr) {
            commandBuffer->EndQuery(queryPool, 0);
        }
    }
} // namespace sky
