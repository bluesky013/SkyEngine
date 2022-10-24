//
// Created by Zach Lee on 2022/6/16.
//

#include <EngineRoot.h>
#include <core/util/Memory.h>
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

    void VulkanDescriptorSample::SetupDescriptorSet()
    {
        drv::DescriptorSetLayout::Descriptor setLayoutInfo = {};
        setLayoutInfo.bindings.emplace(0, drv::DescriptorSetLayout::SetBinding{VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT});
        setLayoutInfo.bindings.emplace(1, drv::DescriptorSetLayout::SetBinding{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT});
        setLayoutInfo.bindings.emplace(2, drv::DescriptorSetLayout::SetBinding{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT});
        setLayoutInfo.bindings.emplace(3, drv::DescriptorSetLayout::SetBinding{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT});
        setLayoutInfo.bindings.emplace(4, drv::DescriptorSetLayout::SetBinding{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_FRAGMENT_BIT});
        setLayoutInfo.bindings.emplace(5, drv::DescriptorSetLayout::SetBinding{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT});
        setLayoutInfo.bindings.emplace(6, drv::DescriptorSetLayout::SetBinding{VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT});
        setLayoutInfo.bindings.emplace(7, drv::DescriptorSetLayout::SetBinding{VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT});

        drv::PipelineLayout::Descriptor pipelineLayoutInfo = {};
        pipelineLayoutInfo.desLayouts.emplace_back(setLayoutInfo);

        pipelineLayout = device->CreateDeviceObject<drv::PipelineLayout>(pipelineLayoutInfo);

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
        auto writer = set->CreateWriter();
        auto &limits = device->GetProperties().limits;

        {
            drv::Image::Descriptor imageInfo = {};
            imageInfo.format                 = VK_FORMAT_R8G8B8A8_UNORM;
            imageInfo.extent                 = {128, 128, 1};
            imageInfo.usage                  = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
            imageInfo.memory                 = VMA_MEMORY_USAGE_GPU_ONLY;
            imageInfo.arrayLayers            = 3;
            inputImage0                      = device->CreateDeviceObject<drv::Image>(imageInfo);

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

            drv::Buffer::Descriptor bufferInfo = {};
            bufferInfo.usage                   = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            bufferInfo.memory                  = VMA_MEMORY_USAGE_CPU_ONLY;
            bufferInfo.size                    = data.size();
            auto     staging                   = device->CreateDeviceObject<drv::Buffer>(bufferInfo);
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

            drv::ImageView::Descriptor viewInfo = {};
            viewInfo.format                     = VK_FORMAT_R8G8B8A8_UNORM;

            viewInfo.subResourceRange.baseArrayLayer = 0;
            imageView0                          = drv::ImageView::CreateImageView(inputImage0, viewInfo);

            viewInfo.subResourceRange.baseArrayLayer = 1;
            imageView1                          = drv::ImageView::CreateImageView(inputImage0, viewInfo);

            viewInfo.subResourceRange.baseArrayLayer = 2;
            imageView2                          = drv::ImageView::CreateImageView(inputImage0, viewInfo);

            sampler = device->CreateDeviceObject<drv::Sampler>({});

            writer.Write(0, VK_DESCRIPTOR_TYPE_SAMPLER, {}, sampler);
            writer.Write(1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, imageView0, {});
            writer.Write(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, imageView1, sampler);
            writer.Write(3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, imageView2, {});
        }

        {
            drv::Buffer::Descriptor bufferInfo = {};
            bufferInfo.usage                   = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
            bufferInfo.memory                  = VMA_MEMORY_USAGE_CPU_TO_GPU;
            {
                alignedSize                        = Align(sizeof(Ubo), static_cast<uint32_t>(limits.minUniformBufferOffsetAlignment));
                bufferInfo.size                    = 2 * alignedSize;
                uniformBuffer                      = device->CreateDeviceObject<drv::Buffer>(bufferInfo);
                writer.Write(4, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, uniformBuffer, 0, sizeof(Ubo));
            }

            {
                bufferInfo.size                    = 2 * sizeof(float);
                constantBuffer                     = device->CreateDeviceObject<drv::Buffer>(bufferInfo);
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

            drv::Buffer::Descriptor bufferInfo = {};
            bufferInfo.usage                   = VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT;
            bufferInfo.memory                  = VMA_MEMORY_USAGE_CPU_TO_GPU;
            bufferInfo.size                    = limits.minTexelBufferOffsetAlignment * 2;
            texelBuffer = device->CreateDeviceObject<drv::Buffer>(bufferInfo);
            uint8_t *ptr = texelBuffer->Map();
            memcpy(ptr, texelData, sizeof(texelData));
            texelBuffer->UnMap();

            drv::BufferView::Descriptor viewInfo = {};
            viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;

            {
                viewInfo.offset = 0;
                viewInfo.range  = sizeof(texelData);
                bufferView0 = drv::BufferView::CreateBufferView(texelBuffer, viewInfo);
                writer.Write(6, VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, bufferView0);
            }
            {
                viewInfo.offset = bufferInfo.size / 2;
                viewInfo.range  = sizeof(texelData);
                bufferView1 = drv::BufferView::CreateBufferView(texelBuffer, viewInfo);
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

    void VulkanDescriptorSample::Tick(float delta)
    {
        VulkanSampleBase::Tick(delta);

        UpdateDynamicBuffer();

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

        setBinder->SetOffset(0, 4, frameIndex * alignedSize);

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
