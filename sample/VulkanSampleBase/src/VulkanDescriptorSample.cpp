//
// Created by Zach Lee on 2022/6/16.
//

#include <EngineRoot.h>
#include <core/util/Memory.h>
#include "VulkanDescriptorSample.h"

namespace sky {

    void VulkanDescriptorSample::SetupPso()
    {
        vertexInput = vk::VertexInput::Builder().Begin().Build();

        vk::GraphicsPipeline::Program program;
        vs = LoadShader(VK_SHADER_STAGE_VERTEX_BIT, ENGINE_ROOT + "/assets/shaders/output/Descriptor.vert.spv");
        fs = LoadShader(VK_SHADER_STAGE_FRAGMENT_BIT, ENGINE_ROOT + "/assets/shaders/output/Descriptor.frag.spv");
        program.shaders.emplace_back(vs);
        program.shaders.emplace_back(fs);

        vk::GraphicsPipeline::State state = {};
        state.raster.cullMode = VK_CULL_MODE_NONE;


        state.blends.blendStates.emplace_back(vk::GraphicsPipeline::BlendState{});
        state.blends.blendStates.back().blendEnable = VK_TRUE;
        state.blends.blendStates.back().srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        state.blends.blendStates.back().dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        state.blends.blendStates.back().srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        state.blends.blendStates.back().dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;

        vk::GraphicsPipeline::VkDescriptor psoDesc = {};
        psoDesc.program                           = &program;
        psoDesc.state                             = &state;
        psoDesc.pipelineLayout                    = pipelineLayout;
        psoDesc.vertexInput                       = vertexInput;
        psoDesc.renderPass                        = renderPass;
        pso                                       = device->CreateDeviceObject<vk::GraphicsPipeline>(psoDesc);
    }

    void VulkanDescriptorSample::SetupDescriptorSet()
    {
        vk::DescriptorSetLayout::VkDescriptor setLayoutInfo = {};
        setLayoutInfo.bindings.emplace(0, vk::DescriptorSetLayout::SetBinding{VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT});
        setLayoutInfo.bindings.emplace(1, vk::DescriptorSetLayout::SetBinding{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT});
        setLayoutInfo.bindings.emplace(2, vk::DescriptorSetLayout::SetBinding{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT});
        setLayoutInfo.bindings.emplace(3, vk::DescriptorSetLayout::SetBinding{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT});
        setLayoutInfo.bindings.emplace(4, vk::DescriptorSetLayout::SetBinding{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_FRAGMENT_BIT});
        setLayoutInfo.bindings.emplace(5, vk::DescriptorSetLayout::SetBinding{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT});
        setLayoutInfo.bindings.emplace(6,
                                       vk::DescriptorSetLayout::SetBinding{VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT});
        setLayoutInfo.bindings.emplace(7,
                                       vk::DescriptorSetLayout::SetBinding{VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT});

        vk::PipelineLayout::VkDescriptor pipelineLayoutInfo = {};
        pipelineLayoutInfo.desLayouts.emplace_back(setLayoutInfo);

        pipelineLayout = device->CreateDeviceObject<vk::PipelineLayout>(pipelineLayoutInfo);

        VkDescriptorPoolSize sizes[] =
            {
                {VK_DESCRIPTOR_TYPE_SAMPLER,                1},
                {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,          1},
                {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1},
                {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          1},
                {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         1},
                {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1},
                {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         1},
                {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,   1},
                {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,   1},
            };

        vk::DescriptorSetPool::VkDescriptor poolInfo = {};
        poolInfo.maxSets = 1;
        poolInfo.num = 1;
        poolInfo.sizes = sizes;
        setPool = device->CreateDeviceObject<vk::DescriptorSetPool>(poolInfo);
        set = pipelineLayout->Allocate(setPool, 0);

        setBinder = std::make_shared<vk::DescriptorSetBinder>();
        setBinder->SetBindPoint(VK_PIPELINE_BIND_POINT_GRAPHICS);
        setBinder->SetPipelineLayout(pipelineLayout);
        setBinder->BindSet(0, set);
    }

    void VulkanDescriptorSample::SetupResources()
    {
        auto writer = set->CreateWriter();
        auto &limits = device->GetProperties().limits;

        {
            vk::Image::VkDescriptor imageInfo = {};
            imageInfo.format                 = VK_FORMAT_R8G8B8A8_UNORM;
            imageInfo.extent                 = {128, 128, 1};
            imageInfo.usage                  = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
            imageInfo.memory                 = VMA_MEMORY_USAGE_GPU_ONLY;
            imageInfo.arrayLayers            = 3;
            inputImage0                      = device->CreateDeviceObject<vk::Image>(imageInfo);

            uint32_t             imageSize = 128 * 128 * 4;
            std::vector<uint8_t> data(imageSize * 2, 0);
            for (uint32_t i = 0; i < imageInfo.extent.width; ++i) {
                for (uint32_t j = 0; j < imageInfo.extent.height; ++j) {
                    data[(i * imageInfo.extent.width + j) * 4 + 0] = i * 2 * 9973;
                    data[(i * imageInfo.extent.width + j) * 4 + 1] = j * 2 * 9973;
                    data[(i * imageInfo.extent.width + j) * 4 + 2] = 0;
                    data[(i * imageInfo.extent.width + j) * 4 + 3] = 255;
                }
            }
            for (uint32_t i = 0; i < imageInfo.extent.width; ++i) {
                for (uint32_t j = 0; j < imageInfo.extent.height; ++j) {
                    float xOff = i - imageInfo.extent.width / 2.f;
                    float yOff = j - imageInfo.extent.height / 2.f;
                    float dist = sqrt(xOff * xOff + yOff * yOff);
                    if (dist > imageInfo.extent.width / 2.f) {
                        data[imageSize + (i * imageInfo.extent.width + j) * 4 + 0] = 0;
                        data[imageSize + (i * imageInfo.extent.width + j) * 4 + 1] = 0;
                        data[imageSize + (i * imageInfo.extent.width + j) * 4 + 2] = 0;
                        data[imageSize + (i * imageInfo.extent.width + j) * 4 + 3] = 255;
                    } else {
                        data[imageSize + (i * imageInfo.extent.width + j) * 4 + 0] = static_cast<uint8_t>(dist * 2);
                        data[imageSize + (i * imageInfo.extent.width + j) * 4 + 1] = static_cast<uint8_t>(dist * 2);
                        data[imageSize + (i * imageInfo.extent.width + j) * 4 + 2] = static_cast<uint8_t>(dist * 2);
                        data[imageSize + (i * imageInfo.extent.width + j) * 4 + 3] = 255;
                    }
                }
            }

            vk::Buffer::VkDescriptor bufferInfo = {};
            bufferInfo.usage                   = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            bufferInfo.memory                  = VMA_MEMORY_USAGE_CPU_ONLY;
            bufferInfo.size                    = data.size();
            auto     staging                   = device->CreateDeviceObject<vk::Buffer>(bufferInfo);
            uint8_t *dst                       = staging->Map();
            memcpy(dst, data.data(), data.size());
            staging->UnMap();

            VkBufferImageCopy copyInfo = {};
            copyInfo.bufferOffset      = 0;
            copyInfo.imageSubresource  = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 2};
            copyInfo.imageExtent       = imageInfo.extent;

            auto cmd = device->GetGraphicsQueue()->AllocateCommandBuffer({});
            cmd->Begin();

            cmd->ImageBarrier(inputImage0, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 2},
                {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_NONE, VK_ACCESS_TRANSFER_WRITE_BIT},
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

            cmd->Copy(staging, inputImage0, copyInfo);
            cmd->ImageBarrier(inputImage0, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 2},
                {VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT},
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

            cmd->ImageBarrier(inputImage0, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 2, 1},
                {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_NONE, VK_ACCESS_SHADER_WRITE_BIT},
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

            cmd->End();
            cmd->Submit(*graphicsQueue, {});
            cmd->Wait();

            vk::ImageView::VkDescriptor viewInfo = {};
            viewInfo.format                     = VK_FORMAT_R8G8B8A8_UNORM;

            viewInfo.subResourceRange.baseArrayLayer = 0;
            imageView0                          = vk::ImageView::CreateImageView(inputImage0, viewInfo);

            viewInfo.subResourceRange.baseArrayLayer = 1;
            imageView1                          = vk::ImageView::CreateImageView(inputImage0, viewInfo);

            viewInfo.subResourceRange.baseArrayLayer = 2;
            imageView2                          = vk::ImageView::CreateImageView(inputImage0, viewInfo);

            sampler = device->CreateDeviceObject<vk::Sampler>(vk::Sampler::VkDescriptor{});

            writer.Write(0, VK_DESCRIPTOR_TYPE_SAMPLER, {}, sampler);
            writer.Write(1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, imageView0, {});
            writer.Write(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, imageView1, sampler);
            writer.Write(3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, imageView2, {});
        }

        {
            vk::Buffer::VkDescriptor bufferInfo = {};
            bufferInfo.usage                   = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
            bufferInfo.memory                  = VMA_MEMORY_USAGE_CPU_TO_GPU;
            {
                alignedSize                        = Align(static_cast<uint32_t>(sizeof(Ubo)), static_cast<uint32_t>(limits.minUniformBufferOffsetAlignment));
                bufferInfo.size                    = 2 * alignedSize;
                uniformBuffer                      = device->CreateDeviceObject<vk::Buffer>(bufferInfo);
                writer.Write(4, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, uniformBuffer, 0, sizeof(Ubo));
            }

            {
                bufferInfo.size                    = 2 * sizeof(float);
                constantBuffer                     = device->CreateDeviceObject<vk::Buffer>(bufferInfo);
                float *ptr = reinterpret_cast<float *>(constantBuffer->Map());
                ptr[0] = 128;
                ptr[1] = 128;
                constantBuffer->UnMap();
                writer.Write(5, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, constantBuffer, 0, bufferInfo.size);
            }
        }

        {
            uint8_t texelData[] = {
                255, 255, 255, 255,
                0, 0, 0, 0,
            };

            vk::Buffer::VkDescriptor bufferInfo = {};
            bufferInfo.usage                   = VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT;
            bufferInfo.memory                  = VMA_MEMORY_USAGE_CPU_TO_GPU;
            bufferInfo.size                    = limits.minTexelBufferOffsetAlignment * 2;
            texelBuffer = device->CreateDeviceObject<vk::Buffer>(bufferInfo);
            uint8_t *ptr = texelBuffer->Map();
            memcpy(ptr, texelData, sizeof(texelData));
            texelBuffer->UnMap();

            vk::BufferView::VkDescriptor viewInfo = {};
            viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;

            {
                viewInfo.offset = 0;
                viewInfo.range  = sizeof(texelData);
                bufferView0 = vk::BufferView::CreateBufferView(texelBuffer, viewInfo);
                writer.Write(6, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, bufferView0);
            }
            {
                viewInfo.offset = bufferInfo.size / 2;
                viewInfo.range  = sizeof(texelData);
                bufferView1 = vk::BufferView::CreateBufferView(texelBuffer, viewInfo);
                writer.Write(7, VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, bufferView1);
            }
        }

        writer.Update();
    }

    void VulkanDescriptorSample::OnMouseMove(int32_t x, int32_t y)
    {
        mouseX = x;
        mouseY = y;
    }

    void VulkanDescriptorSample::OnMouseWheel(int32_t wheelX, int32_t wheelY)
    {
        scale += wheelY;
        scale = std::clamp(scale, 1, 32);
    }

    void VulkanDescriptorSample::UpdateDynamicBuffer()
    {
        if (uniformBuffer) {
            Ubo *ptr = reinterpret_cast<Ubo *>(uniformBuffer->Map() + frameIndex * alignedSize);
            ptr->x = static_cast<float>(mouseX);
            ptr->y = static_cast<float>(mouseY);
            ptr->scaleX = static_cast<float>(scale);
            uniformBuffer->UnMap();
        }
    }

    void VulkanDescriptorSample::OnTick(float delta)
    {
        UpdateDynamicBuffer();

        uint32_t imageIndex = 0;
        swapChain->AcquireNext(imageAvailable, imageIndex);

        commandBuffer->Wait();
        commandBuffer->Begin();

        auto cmd             = commandBuffer->GetNativeHandle();
        auto graphicsEncoder = commandBuffer->EncodeVkGraphics();

        VkClearValue clearValue     = {};
        clearValue.color.float32[0] = 0.2f;
        clearValue.color.float32[1] = 0.2f;
        clearValue.color.float32[2] = 0.2f;
        clearValue.color.float32[3] = 1.f;

        rhi::CmdDraw args         = {};
        args.type                 = rhi::CmdDrawType::LINEAR;
        args.linear.firstVertex   = 0;
        args.linear.firstInstance = 0;
        args.linear.vertexCount   = 6;
        args.linear.instanceCount = 1;

        setBinder->SetOffset(0, 4, frameIndex * alignedSize);

        vk::PassBeginInfo beginInfo = {};
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

        vk::CommandBuffer::SubmitInfo submitInfo = {};
        submitInfo.submitSignals.emplace_back(renderFinish);
        submitInfo.waits.emplace_back(
            std::pair<VkPipelineStageFlags, vk::SemaphorePtr>{VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, imageAvailable});

        commandBuffer->Submit(*graphicsQueue, submitInfo);

        vk::SwapChain::PresentInfo presentInfo = {};
        presentInfo.imageIndex                  = imageIndex;
        presentInfo.signals.emplace_back(renderFinish);
        swapChain->Present(presentInfo);
        VulkanSampleBase::OnTick(delta);
    }

    void VulkanDescriptorSample::OnStart()
    {
        VulkanSampleBase::OnStart();
        SetupDescriptorSet();
        SetupPso();
        SetupResources();
    }

    void VulkanDescriptorSample::OnStop()
    {
        device->WaitIdle();
        pipelineLayout      = nullptr;
        descriptorSetLayout = nullptr;
        pso                 = nullptr;
        set                 = nullptr;
        setPool             = nullptr;
        setBinder           = nullptr;
        vs                  = nullptr;
        fs                  = nullptr;
        vertexInput         = nullptr;
        sampler             = nullptr;
        inputImage0         = nullptr;
        imageView0          = nullptr;
        imageView1          = nullptr;
        imageView2          = nullptr;
        storageImage        = nullptr;
        storageImageView    = nullptr;
        uniformBuffer       = nullptr;
        constantBuffer      = nullptr;
        texelBuffer         = nullptr;
        storageBuffer       = nullptr;
        bufferView0         = nullptr;
        bufferView1         = nullptr;
        VulkanSampleBase::OnStop();
    }
} // namespace sky
