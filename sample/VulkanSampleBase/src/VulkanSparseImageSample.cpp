//
// Created by Zach Lee on 2022/11/30.
//

#include <EngineRoot.h>
#include "VulkanSparseImageSample.h"

namespace sky {

    void VulkanSparseImageSample::InitSparseImage()
    {
        vk::SparseImage::VkDescriptor imageDesc = {};
        imageDesc.imageType   = VK_IMAGE_TYPE_2D;
        imageDesc.format      = VK_FORMAT_R8G8B8A8_UNORM;
        imageDesc.extent      = {128 * 100, 128 * 100, 1};
        imageDesc.mipLevels   = 1;
        imageDesc.arrayLayers = 1;
        imageDesc.usage       = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        imageDesc.memory      = VMA_MEMORY_USAGE_GPU_ONLY;
        imageDesc.viewType    = VK_IMAGE_VIEW_TYPE_2D;
        imageDesc.pageSize    = {128, 128, 1};

        sparseImage = device->CreateDeviceObject<vk::SparseImage>(imageDesc);
    }

    void VulkanSparseImageSample::OnStart()
    {
        VulkanSampleBase::OnStart();
        vertexInput = vk::VertexInput::Builder().Begin().Build();

        vk::PipelineLayout::VkDescriptor pDesc = {};
        pipelineLayout                       = device->CreateDeviceObject<vk::PipelineLayout>(pDesc);

        vk::GraphicsPipeline::Program program;
        vs = LoadShader(VK_SHADER_STAGE_VERTEX_BIT, ENGINE_ROOT + "/assets/shaders/output/Triangle.vert.spv");
        fs = LoadShader(VK_SHADER_STAGE_FRAGMENT_BIT, ENGINE_ROOT + "/assets/shaders/output/Triangle.frag.spv");
        program.shaders.emplace_back(vs);
        program.shaders.emplace_back(fs);

        vk::GraphicsPipeline::State state = {};
        state.raster.cullMode             = VK_CULL_MODE_NONE;
        state.blends.blendStates.emplace_back(vk::GraphicsPipeline::BlendState{});

        vk::GraphicsPipeline::VkDescriptor psoDesc = {};
        psoDesc.program                          = &program;
        psoDesc.state                            = &state;
        psoDesc.pipelineLayout                   = pipelineLayout;
        psoDesc.vertexInput                      = vertexInput;
        psoDesc.renderPass                       = renderPass;
        pso                                      = device->CreateDeviceObject<vk::GraphicsPipeline>(psoDesc);

        InitSparseImage();
    }

    void VulkanSparseImageSample::OnStop()
    {
        device->WaitIdle();
        pso            = nullptr;
        pipelineLayout = nullptr;
        vs             = nullptr;
        fs             = nullptr;
        vertexInput    = nullptr;
        VulkanSampleBase::OnStop();
    }

    void VulkanSparseImageSample::OnTick(float delta)
    {
        uint32_t imageIndex = 0;
        swapChain->AcquireNext(imageAvailable, imageIndex);

        commandBuffer->Wait();
        commandBuffer->Begin();

        auto cmd             = commandBuffer->GetNativeHandle();
        auto graphicsEncoder = commandBuffer->EncodeGraphics();

        VkClearValue clearValue     = {};
        clearValue.color.float32[0] = 0.f;
        clearValue.color.float32[1] = 0.f;
        clearValue.color.float32[2] = 0.f;
        clearValue.color.float32[3] = 1.f;

        vk::CmdDraw args          = {};
        args.type                 = vk::CmdDrawType::LINEAR;
        args.linear.firstVertex   = 0;
        args.linear.firstInstance = 0;
        args.linear.vertexCount   = 3;
        args.linear.instanceCount = 1;

        vk::DrawItem item = {};
        item.pso          = pso;
        item.drawArgs     = args;

        vk::PassBeginInfo beginInfo = {};
        beginInfo.frameBuffer       = frameBuffers[imageIndex];
        beginInfo.renderPass        = renderPass;
        beginInfo.clearValueCount   = 1;
        beginInfo.clearValues       = &clearValue;

        graphicsEncoder.BeginPass(beginInfo);
        graphicsEncoder.Encode(item);
        graphicsEncoder.EndPass();

        commandBuffer->End();

        vk::CommandBuffer::SubmitInfo submitInfo = {};
        submitInfo.submitSignals.emplace_back(renderFinish);
        submitInfo.waits.emplace_back(
            std::pair<VkPipelineStageFlags, vk::SemaphorePtr>{VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, imageAvailable});

        commandBuffer->Submit(*graphicsQueue, submitInfo);

        vk::SwapChain::PresentInfo presentInfo = {};
        presentInfo.imageIndex                 = imageIndex;
        presentInfo.signals.emplace_back(renderFinish);
        swapChain->Present(presentInfo);

        VulkanSampleBase::OnTick(delta);
    }
} // namespace sky
