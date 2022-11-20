//
// Created by Zach Lee on 2022/6/16.
//

#include "VulkanAsyncUploadSample.h"

#include <EngineRoot.h>
#include <core/util/Memory.h>
#include <core/math/Vector4.h>
#include <core/file/FileIO.h>
#include <core/logger/Logger.h>
#include <rhi/Decode.h>
#include <future>
#include <vulkan/Conversion.h>

namespace sky {

    struct ColorU8 {
        uint8_t value[4];
    };

    void VulkanAsyncUploadSample::SetupPso()
    {
        vertexInput = vk::VertexInput::Builder().Begin().Build();

        vk::GraphicsPipeline::Program program;
        vs = LoadShader(VK_SHADER_STAGE_VERTEX_BIT, ENGINE_ROOT + "/assets/shaders/output/AsyncUpload.vert.spv");
        fs = LoadShader(VK_SHADER_STAGE_FRAGMENT_BIT, ENGINE_ROOT + "/assets/shaders/output/AsyncUpload.frag.spv");
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

    void VulkanAsyncUploadSample::SetupDescriptorSet()
    {
        vk::DescriptorSetLayout::VkDescriptor setLayoutInfo = {};
        setLayoutInfo.bindings.emplace(0, vk::DescriptorSetLayout::SetBinding{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT});

        vk::PipelineLayout::VkDescriptor pipelineLayoutInfo = {};
        pipelineLayoutInfo.desLayouts.emplace_back(setLayoutInfo);

        pipelineLayout = device->CreateDeviceObject<vk::PipelineLayout>(pipelineLayoutInfo);

        VkDescriptorPoolSize sizes[] =
            {
                {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1},
            };

        vk::DescriptorSetPool::VkDescriptor poolInfo = {};
        poolInfo.maxSets = 1;
        poolInfo.num = 1;
        poolInfo.sizes = sizes;
        setPool = device->CreateDeviceObject<vk::DescriptorSetPool>(poolInfo);
        set = pipelineLayout->Allocate(setPool, 0);

        object.setBinder = std::make_shared<vk::DescriptorSetBinder>();
        object.setBinder->SetBindPoint(VK_PIPELINE_BIND_POINT_GRAPHICS);
        object.setBinder->SetPipelineLayout(pipelineLayout);
        object.setBinder->BindSet(0, set);
    }

    static uint32_t Ceil(uint32_t v0, uint32_t v1) {
        return (v0 + v1 - 1) / v1;
    }

    void VulkanAsyncUploadSample::SetupResources()
    {
        std::vector<uint8_t> data;
        ReadBin(ENGINE_ROOT + "/assets/images/Logo.dds", data);

        vk::Image::Descriptor tmpDesc = {};
        std::vector<rhi::ImageUploadRequest> requests;
        ProcessDDS(data.data(), data.size(), tmpDesc, requests);

        vk::Image::VkDescriptor imageDesc = {};
        imageDesc.imageType   = vk::FromRHI(tmpDesc.imageType);
        imageDesc.format      = vk::FromRHI(tmpDesc.format);
        imageDesc.extent      = {tmpDesc.extent.width, tmpDesc.extent.height, tmpDesc.extent.depth};
        imageDesc.mipLevels   = tmpDesc.mipLevels;
        imageDesc.arrayLayers = tmpDesc.arrayLayers;

        imageDesc.usage  = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        imageDesc.memory = VMA_MEMORY_USAGE_GPU_ONLY;
        image            = device->CreateDeviceObject<vk::Image>(imageDesc);

        vk::ImageView::VkDescriptor viewDesc = {};
        viewDesc.format = imageDesc.format;
        view = vk::ImageView::CreateImageView(image, viewDesc);

        vk::Sampler::VkDescriptor samplerDesc = {};
        samplerDesc.minLod = 0.f;
        samplerDesc.maxLod = 13;
        sampler = device->CreateDeviceObject<vk::Sampler>(samplerDesc);

        object.future = std::async([this, &requests, tmpDesc]() {
            std::vector<vk::BufferPtr> stagingBuffers;

            auto     uploadQueue = device->GetAsyncTransferQueue()->GetQueue();
            auto *imageInfo = GetImageInfoByFormat(tmpDesc.format);

            auto cmd = uploadQueue->AllocateCommandBuffer({});
            cmd->Begin();

            for (auto &request : requests) {
                VkImageSubresourceRange subResourceRange = {};
                subResourceRange.aspectMask              = VK_IMAGE_ASPECT_COLOR_BIT;
                subResourceRange.baseMipLevel            = request.mipLevel;
                subResourceRange.levelCount              = 1;
                subResourceRange.baseArrayLayer          = 0;
                subResourceRange.layerCount              = 1;
                vk::Barrier barrier                     = {};
                {
                    barrier.srcStageMask  = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                    barrier.dstStageMask  = VK_PIPELINE_STAGE_TRANSFER_BIT;
                    barrier.srcAccessMask = 0;
                    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                    cmd->ImageBarrier(image, subResourceRange, barrier, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
                }

                uint32_t width  = std::max(tmpDesc.extent.width >> request.mipLevel, 1U);
                uint32_t height = std::max(tmpDesc.extent.height >> request.mipLevel, 1U);

                uint32_t rowLength   = Ceil(width, imageInfo->blockWidth);
                uint32_t imageHeight = Ceil(height, imageInfo->blockHeight);

                static constexpr uint32_t STAGING_BLOCK_SIZE = 1 * 1024 * 1024;
                uint32_t                  blockNum           = rowLength * imageHeight;
                uint32_t                  srcSize            = blockNum * imageInfo->blockSize;
                uint32_t                  rowBlockSize       = rowLength * imageInfo->blockSize;
                uint32_t                  copyBlockHeight    = STAGING_BLOCK_SIZE / rowBlockSize;
                uint32_t                  copyNum            = Ceil(imageHeight, copyBlockHeight);

                uint32_t bufferStep = copyBlockHeight * rowBlockSize;
                uint32_t heightStep = copyBlockHeight * imageInfo->blockHeight;

                for (uint32_t i = 0; i < copyNum; ++i) {
                    vk::Buffer::VkDescriptor bufferDesc = {};
                    bufferDesc.size                    = STAGING_BLOCK_SIZE;
                    bufferDesc.usage                   = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
                    bufferDesc.memory                  = VMA_MEMORY_USAGE_CPU_ONLY;
                    auto stagingBuffer                 = device->CreateDeviceObject<vk::Buffer>(bufferDesc);
                    stagingBuffers.emplace_back(stagingBuffer);

                    auto     ptr      = stagingBuffer->Map();
                    uint32_t copySize = std::min(bufferStep, srcSize - bufferStep * i);
                    memcpy(ptr, request.data + request.offset + bufferStep * i, copySize);
                    stagingBuffer->UnMap();

                    VkBufferImageCopy copy = {};
                    copy.bufferOffset      = 0;
                    copy.bufferRowLength   = rowLength * imageInfo->blockWidth;
                    copy.bufferImageHeight = imageHeight * imageInfo->blockHeight;
                    copy.imageSubresource  = {VK_IMAGE_ASPECT_COLOR_BIT, request.mipLevel, 0, 1};
                    copy.imageOffset       = {0, static_cast<int32_t>(heightStep * i), 0};
                    copy.imageExtent       = {width, std::min(height - heightStep * i, heightStep), 1};

                    cmd->Copy(stagingBuffer, image, copy);
                }

                {
                    barrier.srcStageMask  = VK_PIPELINE_STAGE_TRANSFER_BIT;
                    barrier.dstStageMask  = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                    barrier.dstAccessMask = 0;
                    cmd->ImageBarrier(image, subResourceRange, barrier, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                }
            }

            cmd->End();
            cmd->Submit(*uploadQueue, {});
            cmd->Wait();

            auto writer = set->CreateWriter();
            writer.Write(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, view, sampler);
            writer.Update();

            object.isReady = true;
        });
        object.future.wait();
    }

    void VulkanAsyncUploadSample::OnTick(float delta)
    {
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

        vk::PassBeginInfo beginInfo = {};
        beginInfo.frameBuffer        = frameBuffers[imageIndex];
        beginInfo.renderPass         = renderPass;
        beginInfo.clearValueCount    = 1;
        beginInfo.clearValues        = &clearValue;

        graphicsEncoder.BeginPass(beginInfo);

        vk::CmdDrawLinear drawLinear = {};
        drawLinear.firstVertex   = 0;
        drawLinear.firstInstance = 0;
        drawLinear.vertexCount   = 6;
        drawLinear.instanceCount = 13;

        graphicsEncoder.BindPipeline(pso);

        if (object.isReady) {
            graphicsEncoder.BindShaderResource(object.setBinder);
            graphicsEncoder.DrawLinear(drawLinear);
        }

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

    void VulkanAsyncUploadSample::OnStart()
    {
        VulkanSampleBase::OnStart();
        SetupDescriptorSet();
        SetupPso();
        SetupResources();
    }

    void VulkanAsyncUploadSample::OnStop()
    {
        device->WaitIdle();

        pipelineLayout      = nullptr;
        descriptorSetLayout = nullptr;
        pso                 = nullptr;
        set                 = nullptr;
        setPool             = nullptr;
        vs                  = nullptr;
        fs                  = nullptr;
        vertexInput         = nullptr;
        sampler             = nullptr;
        view                = nullptr;
        image               = nullptr;
        object.setBinder    = nullptr;

        VulkanSampleBase::OnStop();
    }

} // namespace sky