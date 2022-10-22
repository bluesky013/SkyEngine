//
// Created by Zach Lee on 2022/6/16.
//

#include "VulkanAsyncUploadSample.h"

#include <EngineRoot.h>
#include <core/util/Memory.h>
#include <core/math/Vector4.h>
#include <future>

namespace sky {

    struct ColorU8 {
        uint8_t value[4];
    };

    void VulkanAsyncUploadSample::SetupPso()
    {
        vertexInput = drv::VertexInput::Builder().Begin().Build();

        drv::GraphicsPipeline::Program program;
        vs = LoadShader(VK_SHADER_STAGE_VERTEX_BIT, ENGINE_ROOT + "/assets/shaders/output/AsyncUpload.vert.spv");
        fs = LoadShader(VK_SHADER_STAGE_FRAGMENT_BIT, ENGINE_ROOT + "/assets/shaders/output/AsyncUpload.frag.spv");
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

    void VulkanAsyncUploadSample::SetupDescriptorSet()
    {
        drv::DescriptorSetLayout::Descriptor setLayoutInfo = {};
        setLayoutInfo.bindings.emplace(0, drv::DescriptorSetLayout::SetBinding{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT});

        drv::PipelineLayout::Descriptor pipelineLayoutInfo = {};
        pipelineLayoutInfo.desLayouts.emplace_back(setLayoutInfo);

        pipelineLayout = device->CreateDeviceObject<drv::PipelineLayout>(pipelineLayoutInfo);

        VkDescriptorPoolSize sizes[] =
            {
                {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1},
            };

        drv::DescriptorSetPool::Descriptor poolInfo = {};
        poolInfo.maxSets = 1;
        poolInfo.num = 1;
        poolInfo.sizes = sizes;
        setPool = device->CreateDeviceObject<drv::DescriptorSetPool>(poolInfo);
        set = pipelineLayout->Allocate(setPool, 0);

        object.setBinder = std::make_shared<drv::DescriptorSetBinder>();
        object.setBinder->SetBindPoint(VK_PIPELINE_BIND_POINT_GRAPHICS);
        object.setBinder->SetPipelineLayout(pipelineLayout);
        object.setBinder->BindSet(0, set);
    }

    void VulkanAsyncUploadSample::SetupResources()
    {
        drv::Image::Descriptor imageDesc = {};
        imageDesc.format    = VK_FORMAT_R8G8B8A8_UNORM;
        imageDesc.extent    = {128, 128, 1};
        imageDesc.usage     = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        imageDesc.memory    = VMA_MEMORY_USAGE_GPU_ONLY;
        image = device->CreateDeviceObject<drv::Image>(imageDesc);

        drv::ImageView::Descriptor viewDesc = {};
        viewDesc.format = VK_FORMAT_R8G8B8A8_UNORM;
        view = drv::ImageView::CreateImageView(image, viewDesc);

        sampler = device->CreateDeviceObject<drv::Sampler>({});

        object.future = std::async([this]() {
            auto uploadQueue = device->GetAsyncTransferQueue()->GetQueue();
            drv::Buffer::Descriptor bufferDesc = {};
            bufferDesc.size      = 128 * 128 * 4;
            bufferDesc.usage     = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            bufferDesc.memory    = VMA_MEMORY_USAGE_CPU_ONLY;
            auto stagingBuffer = device->CreateDeviceObject<drv::Buffer>(bufferDesc);
            std::vector<ColorU8> colors(128 * 128);
            for (uint32_t i = 0; i < 128; ++i) {
                for (uint32_t j = 0; j < 128; ++j) {
                    colors[i * 128 + j].value[0] = rand() % 255;
                    colors[i * 128 + j].value[1] = rand() % 255;
                    colors[i * 128 + j].value[2] = rand() % 255;
                    colors[i * 128 + j].value[3] = rand() % 255;
                }
            }

            auto ptr = stagingBuffer->Map();
            memcpy(ptr, colors.data(), colors.size() * sizeof(ColorU8));
            stagingBuffer->UnMap();

            VkImageSubresourceRange subResourceRange = {};
            subResourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            subResourceRange.baseMipLevel = 0;
            subResourceRange.levelCount = 1;
            subResourceRange.baseArrayLayer = 0;
            subResourceRange.layerCount = 1;

            VkBufferImageCopy copy = {};
            copy.bufferOffset = 0;
            copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
            copy.imageOffset = {0, 0, 0};
            copy.imageExtent = {128 , 128, 1};

            drv::Barrier barrier = {};

            auto cmd = uploadQueue->AllocateCommandBuffer({});
            cmd->Begin();
            {
                barrier.srcStageMask  = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                barrier.dstStageMask  = VK_PIPELINE_STAGE_TRANSFER_BIT;
                barrier.srcAccessMask = 0;
                barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                cmd->ImageBarrier(image, subResourceRange, barrier, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
            }

            cmd->Copy(stagingBuffer, image, copy);

            {
                barrier.srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
                barrier.dstStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                barrier.dstAccessMask = 0;
                cmd->ImageBarrier(image, subResourceRange, barrier, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            }

            cmd->End();
            cmd->Submit(*uploadQueue, {});
            cmd->Wait();

            std::this_thread::sleep_for(std::chrono::duration<float, std::milli>(2000));

            auto writer = set->CreateWriter();
            writer.Write(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, view, sampler);
            writer.Update();

            object.isReady = true;
        });
    }

    void VulkanAsyncUploadSample::Tick(float delta)
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

        drv::PassBeginInfo beginInfo = {};
        beginInfo.frameBuffer        = frameBuffers[imageIndex];
        beginInfo.renderPass         = renderPass;
        beginInfo.clearValueCount    = 1;
        beginInfo.clearValues        = &clearValue;

        graphicsEncoder.BeginPass(beginInfo);

        drv::CmdDrawLinear drawLinear = {};
        drawLinear.firstVertex   = 0;
        drawLinear.firstInstance = 0;
        drawLinear.vertexCount   = 6;
        drawLinear.instanceCount = 1;

        graphicsEncoder.BindPipeline(pso);

        if (object.isReady) {
            graphicsEncoder.BindShaderResource(object.setBinder);
            graphicsEncoder.DrawLinear(drawLinear);
        }

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

    void VulkanAsyncUploadSample::OnStart()
    {
        SetupDescriptorSet();
        SetupPso();
        SetupResources();
    }

    void VulkanAsyncUploadSample::OnStop()
    {
    }

} // namespace sky

REGISTER_MODULE(sky::VulkanAsyncUploadSample)
