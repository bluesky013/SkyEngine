//
// Created by Zach Lee on 2022/6/16.
//

#include <EngineRoot.h>
#include "VulkanDescriptorSample.h"

namespace sky {

    void VulkanDescriptorSample::SetupPso()
    {
        vertexInput = drv::VertexInput::Builder().Begin().Build();

        drv::GraphicsPipeline::Program program;
        vs = LoadShader(VK_SHADER_STAGE_VERTEX_BIT, ENGINE_ROOT + "/assets/shaders/output/Descriptor.vert.spv");
        fs = LoadShader(VK_SHADER_STAGE_FRAGMENT_BIT, ENGINE_ROOT + "/assets/shaders/output/Descriptor.frag.spv");
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

    void VulkanDescriptorSample::SetupDescriptorSet()
    {
        drv::DescriptorSetLayout::Descriptor setLayoutInfo = {};
//        setLayoutInfo.bindings.emplace(0, drv::DescriptorSetLayout::SetBinding{VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT});
//        setLayoutInfo.bindings.emplace(1, drv::DescriptorSetLayout::SetBinding{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT});
//        setLayoutInfo.bindings.emplace(2, drv::DescriptorSetLayout::SetBinding{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT});
//        setLayoutInfo.bindings.emplace(3, drv::DescriptorSetLayout::SetBinding{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT});
        setLayoutInfo.bindings.emplace(5, drv::DescriptorSetLayout::SetBinding{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_FRAGMENT_BIT});
//        setLayoutInfo.bindings.emplace(6, drv::DescriptorSetLayout::SetBinding{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT});

        drv::PipelineLayout::Descriptor pipelineLayoutInfo = {};
        pipelineLayoutInfo.desLayouts.emplace_back(setLayoutInfo);

        pipelineLayout = device->CreateDeviceObject<drv::PipelineLayout>(pipelineLayoutInfo);

        VkDescriptorPoolSize sizes[] = {
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
        };

        drv::DescriptorSetPool::Descriptor poolInfo = {};
        poolInfo.maxSets = 1;
        poolInfo.num = 1;
        poolInfo.sizes = sizes;
        setPool = device->CreateDeviceObject<drv::DescriptorSetPool>(poolInfo);
        set = pipelineLayout->Allocate(setPool, 0);

        setBinder = std::make_shared<drv::DescriptorSetBinder>();
        setBinder->SetBindPoint(VK_PIPELINE_BIND_POINT_GRAPHICS);
        setBinder->SetPipelineLayout(pipelineLayout);
        setBinder->BindSet(0, set);
    }

    void VulkanDescriptorSample::SetupResources()
    {
        sampler = device->CreateDeviceObject<drv::Sampler>({});

        drv::Image::Descriptor imageInfo = {};
        imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        imageInfo.extent = {2, 2, 1};
        imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        imageInfo.memory = VMA_MEMORY_USAGE_GPU_ONLY;
        imageInfo.arrayLayers = 2;

        drv::ImageView::Descriptor viewInfo = {};
        viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;

        viewInfo.subResourceRange.baseArrayLayer = 0;
        inputImage0 = device->CreateDeviceObject<drv::Image>(imageInfo);
        inputImageView0 = drv::ImageView::CreateImageView(inputImage0, viewInfo);

        viewInfo.subResourceRange.baseArrayLayer = 1;
        inputImage1 = device->CreateDeviceObject<drv::Image>(imageInfo);
        inputImageView1 = drv::ImageView::CreateImageView(inputImage1, viewInfo);

        drv::Buffer::Descriptor bufferInfo = {};
        bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        bufferInfo.memory = VMA_MEMORY_USAGE_CPU_TO_GPU;
        bufferInfo.size = 2 * 2 * sizeof(float);

        uniformBuffer = device->CreateDeviceObject<drv::Buffer>(bufferInfo);

        auto writer = set->CreateWriter();
        writer.Write(5, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, uniformBuffer, 0, bufferInfo.size);
        writer.Update();
    }

    void VulkanDescriptorSample::OnMouseMove(int32_t x, int32_t y)
    {
        if (swapChain == nullptr || uniformBuffer == nullptr) {
            return;
        }

        float *ptr = reinterpret_cast<float *>(uniformBuffer->Map());
        ptr[2 * frameIndex + 0] = (float)x;
        ptr[2 * frameIndex + 1] = (float)y;

        uniformBuffer->UnMap();
    }

    void VulkanDescriptorSample::Tick(float delta)
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
        args.linear.vertexCount   = 6;
        args.linear.instanceCount = 1;

        setBinder->SetOffset(0, 5, frameIndex * 2 * sizeof(float));

        drv::DrawItem item = {};
        item.pso           = pso;
        item.drawArgs      = args;
        item.shaderResources = setBinder;

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

    void VulkanDescriptorSample::OnStart()
    {
        SetupDescriptorSet();
        SetupPso();
        SetupResources();
    }

    void VulkanDescriptorSample::OnStop()
    {

    }

} // namespace sky

REGISTER_MODULE(sky::VulkanDescriptorSample)