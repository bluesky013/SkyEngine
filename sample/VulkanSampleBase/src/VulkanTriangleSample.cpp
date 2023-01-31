//
// Created by Zach Lee on 2022/6/16.
//

#include <EngineRoot.h>
#include <VulkanTriangleSample.h>

namespace sky {

    void VulkanTriangleSample::OnStart()
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
    }

    void VulkanTriangleSample::OnStop()
    {
        device->WaitIdle();
        pso            = nullptr;
        pipelineLayout = nullptr;
        vs             = nullptr;
        fs             = nullptr;
        vertexInput    = nullptr;
        VulkanSampleBase::OnStop();
    }

    void VulkanTriangleSample::OnTick(float delta)
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

        rhi::CmdDraw args          = {};
        args.type                 = rhi::CmdDrawType::LINEAR;
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
