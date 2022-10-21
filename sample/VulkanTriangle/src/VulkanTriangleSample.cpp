//
// Created by Zach Lee on 2022/6/16.
//

#include <EngineRoot.h>
#include <VulkanTriangleSample.h>
#include <core/file/FileIO.h>

namespace sky {

    void VulkanTriangleSample::OnStart()
    {
        vertexInput = drv::VertexInput::Builder().Begin().Build();

        drv::PipelineLayout::Descriptor pDesc = {};
        pipelineLayout                        = device->CreateDeviceObject<drv::PipelineLayout>(pDesc);

        drv::GraphicsPipeline::Program program;
        vs = LoadShader(VK_SHADER_STAGE_VERTEX_BIT, ENGINE_ROOT + "/assets/shaders/output/Triangle.vert.spv");
        fs = LoadShader(VK_SHADER_STAGE_FRAGMENT_BIT, ENGINE_ROOT + "/assets/shaders/output/Triangle.frag.spv");
        program.shaders.emplace_back(vs);
        program.shaders.emplace_back(fs);

        drv::GraphicsPipeline::State state = {};
        state.raster.cullMode = VK_CULL_MODE_NONE;
        state.blends.blendStates.emplace_back(drv::GraphicsPipeline::BlendState{});

        drv::GraphicsPipeline::Descriptor psoDesc = {};
        psoDesc.program                           = &program;
        psoDesc.state                             = &state;
        psoDesc.pipelineLayout                    = pipelineLayout;
        psoDesc.vertexInput                       = vertexInput;
        psoDesc.renderPass                        = renderPass;
        pso                                       = device->CreateDeviceObject<drv::GraphicsPipeline>(psoDesc);
    }

    void VulkanTriangleSample::OnStop()
    {
    }

    void VulkanTriangleSample::Tick(float delta)
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

        drv::CmdDraw args         = {};
        args.type                 = drv::CmdDrawType::LINEAR;
        args.linear.firstVertex   = 0;
        args.linear.firstInstance = 0;
        args.linear.vertexCount   = 3;
        args.linear.instanceCount = 1;

        drv::DrawItem item = {};
        item.pso           = pso;
        item.drawArgs      = args;

        drv::PassBeginInfo beginInfo = {};
        beginInfo.frameBuffer        = frameBuffers[imageIndex];
        beginInfo.renderPass         = renderPass;
        beginInfo.clearValueCount    = 1;
        beginInfo.clearValues        = &clearValue;

        graphicsEncoder.BeginPass(beginInfo);
        graphicsEncoder.Encode(item);
        graphicsEncoder.EndPass();

        commandBuffer->End();

        drv::CommandBuffer::SubmitInfo submitInfo = {};
        submitInfo.submitSignals.emplace_back(renderFinish);
        submitInfo.waits.emplace_back(
            std::pair<VkPipelineStageFlags, drv::SemaphorePtr>{VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, imageAvailable});

        commandBuffer->Submit(*graphicsQueue, submitInfo);

        drv::SwapChain::PresentInfo presentInfo = {};
        presentInfo.imageIndex                  = imageIndex;
        presentInfo.signals.emplace_back(renderFinish);
        swapChain->Present(presentInfo);
    }
} // namespace sky

REGISTER_MODULE(sky::VulkanTriangleSample)