//
// Created by Zach Lee on 2022/6/16.
//

#include <EngineRoot.h>
#include <core/util/Memory.h>
#include "VulkanBindlessSample.h"

namespace sky {

    struct ColorU8 {
        uint8_t color[4];
    };

    struct ImageInfo {
        VkFormat format;
        VkExtent3D extent;
        ColorU8 color;
    };

    ImageInfo g_infos[VulkanBindlessSample::IMAGE_NUM] = {
        {VK_FORMAT_R8G8B8A8_UNORM, {2, 2, 1}, {255, 0, 0, 255}},
        {VK_FORMAT_B8G8R8A8_UNORM, {4, 4, 1}, {0, 255, 0, 255}},
        {VK_FORMAT_R8G8B8A8_UNORM, {8, 8, 1}, {0, 0, 255, 255}},
        {VK_FORMAT_B8G8R8A8_UNORM, {16, 16, 1}, {255, 255, 255, 255}},
    };

    void VulkanBindlessSample::SetupPso()
    {
        vertexInput = drv::VertexInput::Builder().Begin().Build();

        drv::GraphicsPipeline::Program program;
        vs = LoadShader(VK_SHADER_STAGE_VERTEX_BIT, ENGINE_ROOT + "/assets/shaders/output/Bindless.vert.spv");
        fs = LoadShader(VK_SHADER_STAGE_FRAGMENT_BIT, ENGINE_ROOT + "/assets/shaders/output/Bindless.frag.spv");
        program.shaders.emplace_back(vs);
        program.shaders.emplace_back(fs);

        drv::GraphicsPipeline::State state = {};
        state.raster.cullMode = VK_CULL_MODE_NONE;


        state.blends.blendStates.emplace_back(drv::GraphicsPipeline::BlendState{});
        state.blends.blendStates.back().blendEnable = VK_TRUE;
        state.blends.blendStates.back().srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        state.blends.blendStates.back().dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        state.blends.blendStates.back().srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        state.blends.blendStates.back().dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;

        drv::GraphicsPipeline::Descriptor psoDesc = {};
        psoDesc.program                           = &program;
        psoDesc.state                             = &state;
        psoDesc.pipelineLayout                    = pipelineLayout;
        psoDesc.vertexInput                       = vertexInput;
        psoDesc.renderPass                        = renderPass;
        pso                                       = device->CreateDeviceObject<drv::GraphicsPipeline>(psoDesc);
    }

    void VulkanBindlessSample::SetupDescriptorSet()
    {
        drv::DescriptorSetLayout::Descriptor setLayoutInfo = {};
        setLayoutInfo.bindings.emplace(0, drv::DescriptorSetLayout::SetBinding{VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT});
        setLayoutInfo.bindings.emplace(1, drv::DescriptorSetLayout::SetBinding{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,IMAGE_NUM,VK_SHADER_STAGE_FRAGMENT_BIT});
        setLayoutInfo.bindings.emplace(2, drv::DescriptorSetLayout::SetBinding{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,IMAGE_NUM,VK_SHADER_STAGE_FRAGMENT_BIT, 0, VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT});

        drv::PipelineLayout::Descriptor pipelineLayoutInfo = {};
        pipelineLayoutInfo.desLayouts.emplace_back(setLayoutInfo);

        pipelineLayout = device->CreateDeviceObject<drv::PipelineLayout>(pipelineLayoutInfo);

        VkDescriptorPoolSize sizes[] = {
            {VK_DESCRIPTOR_TYPE_SAMPLER, 1},
            {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, IMAGE_NUM},
        };

        drv::DescriptorSetPool::Descriptor poolInfo = {};
        poolInfo.maxSets                            = 1;
        poolInfo.num                                = 1;
        poolInfo.sizes                              = sizes;
        setPool                                     = device->CreateDeviceObject<drv::DescriptorSetPool>(poolInfo);
        set                                         = pipelineLayout->Allocate(setPool, 0);

        setBinder = std::make_shared<drv::DescriptorSetBinder>();
        setBinder->SetBindPoint(VK_PIPELINE_BIND_POINT_GRAPHICS);
        setBinder->SetPipelineLayout(pipelineLayout);
        setBinder->BindSet(0, set);
    }

    void VulkanBindlessSample::SetupResources()
    {
        auto  writer = set->CreateWriter();
        auto &limits = device->GetProperties().limits;

        sampler = device->CreateDeviceObject<drv::Sampler>({});
        writer.Write(0, VK_DESCRIPTOR_TYPE_SAMPLER, {}, sampler);

        auto cmd = commandPool->Allocate({});
        cmd->Begin();

        drv::Image::Descriptor imageInfo = {};
        imageInfo.memory                 = VMA_MEMORY_USAGE_GPU_ONLY;
        imageInfo.usage                  = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        std::vector<drv::BufferPtr> stagingBuffer;

        drv::ImageView::Descriptor viewInfo = {};
        for (uint32_t i = 0; i < IMAGE_NUM; ++i) {
            imageInfo.format = g_infos[i].format;
            imageInfo.extent = g_infos[i].extent;
            images[i]        = device->CreateDeviceObject<drv::Image>(imageInfo);

            std::vector<ColorU8> data(imageInfo.extent.width * imageInfo.extent.height, g_infos[i].color);

            drv::Buffer::Descriptor bufferInfo = {};
            bufferInfo.usage                   = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            bufferInfo.memory                  = VMA_MEMORY_USAGE_CPU_ONLY;
            bufferInfo.size                    = data.size() * sizeof(ColorU8);
            auto staging                       = device->CreateDeviceObject<drv::Buffer>(bufferInfo);
            stagingBuffer.emplace_back(staging);
            uint8_t *dst = staging->Map();
            memcpy(dst, data.data(), bufferInfo.size);
            staging->UnMap();

            VkBufferImageCopy copyInfo = {};
            copyInfo.bufferOffset      = 0;
            copyInfo.imageSubresource  = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
            copyInfo.imageExtent       = imageInfo.extent;
            cmd->ImageBarrier(images[i], {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
                              {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_NONE, VK_ACCESS_TRANSFER_WRITE_BIT},
                              VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

            cmd->Copy(staging, images[i], copyInfo);
            cmd->ImageBarrier(
                images[i], {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
                {VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT},
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

            viewInfo.format = imageInfo.format;
            imageViews[i]   = drv::ImageView::CreateImageView(images[i], viewInfo);
        }
        cmd->End();
        cmd->Submit(*graphicsQueue, {});
        cmd->Wait();

        for (uint32_t i = 0; i < IMAGE_NUM; ++i) {
            writer.Write(1, VK_DESCRIPTOR_TYPE_SAMPLER, imageViews[i], {}, i);
            writer.Write(2, VK_DESCRIPTOR_TYPE_SAMPLER, imageViews[i], {}, i);
        }
        writer.Update();
    }


    void VulkanBindlessSample::Tick(float delta)
    {
        VulkanSampleBase::Tick(delta);

        uint32_t imageIndex = 0;
        swapChain->AcquireNext(imageAvailable, imageIndex);

        commandBuffer->Wait();
        commandBuffer->Begin();

        auto cmd             = commandBuffer->GetNativeHandle();
        auto graphicsEncoder = commandBuffer->EncodeGraphics();

        VkClearValue clearValue     = {};
        clearValue.color.float32[0] = 0.2f;
        clearValue.color.float32[1] = 0.2f;
        clearValue.color.float32[2] = 0.2f;
        clearValue.color.float32[3] = 1.f;

        drv::CmdDraw args         = {};
        args.type                 = drv::CmdDrawType::LINEAR;
        args.linear.firstVertex   = 0;
        args.linear.firstInstance = 0;
        args.linear.vertexCount   = 6;
        args.linear.instanceCount = 1;

        drv::PassBeginInfo beginInfo = {};
        beginInfo.frameBuffer        = frameBuffers[imageIndex];
        beginInfo.renderPass         = renderPass;
        beginInfo.clearValueCount    = 1;
        beginInfo.clearValues        = &clearValue;

        graphicsEncoder.BeginPass(beginInfo);

        graphicsEncoder.BindPipeline(pso);
        graphicsEncoder.BindShaderResource(setBinder);
        graphicsEncoder.DrawLinear(args.linear);

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

    void VulkanBindlessSample::OnStart()
    {
        SetupDescriptorSet();
        SetupPso();
        SetupResources();
    }

    void VulkanBindlessSample::OnStop()
    {

    }

} // namespace sky

REGISTER_MODULE(sky::VulkanBindlessSample)
