//
// Created by Zach Lee on 2022/6/16.
//

#include "VulkanAsyncUploadSample.h"

#include <EngineRoot.h>
#include <core/util/Memory.h>
#include <core/math/Vector4.h>
#include <core/file/FileIO.h>
#include <core/logger/Logger.h>
#include <vulkan/Decode.h>
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
        std::vector<uint8_t> data;
        ReadBin(ENGINE_ROOT + "/assets/images/Logo.dds", data);

        drv::Image::Descriptor               imageDesc = {};
        std::vector<drv::ImageUploadRequest> requests;
        ProcessDDS(data.data(), data.size(), imageDesc, requests);

        imageDesc.usage  = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        imageDesc.memory = VMA_MEMORY_USAGE_GPU_ONLY;
        image            = device->CreateDeviceObject<drv::Image>(imageDesc);

        drv::ImageView::Descriptor viewDesc = {};
        viewDesc.format                     = imageDesc.format;
        view                                = drv::ImageView::CreateImageView(image, viewDesc);

        drv::Sampler::Descriptor samplerDesc = {};
        samplerDesc.minLod = 0.f;
        samplerDesc.maxLod = 13;
        sampler = device->CreateDeviceObject<drv::Sampler>(samplerDesc);

        object.future = std::async([this, &imageDesc, &requests]() {
            std::vector<drv::BufferPtr> stagingBuffers;

            auto     uploadQueue = device->GetAsyncTransferQueue()->GetQueue();
            auto *imageInfo = drv::GetImageInfoByFormat(imageDesc.format);
            auto bufferBlockLengthFn = [](uint32_t length, uint32_t blockLength) {
                return (length + blockLength - 1) / blockLength;
            };

            auto cmd = uploadQueue->AllocateCommandBuffer({});
            cmd->Begin();

            for (auto &request : requests) {
                VkImageSubresourceRange subResourceRange = {};
                subResourceRange.aspectMask              = VK_IMAGE_ASPECT_COLOR_BIT;
                subResourceRange.baseMipLevel            = request.mipLevel;
                subResourceRange.levelCount              = 1;
                subResourceRange.baseArrayLayer          = 0;
                subResourceRange.layerCount              = 1;
                drv::Barrier barrier                     = {};
                {
                    barrier.srcStageMask  = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                    barrier.dstStageMask  = VK_PIPELINE_STAGE_TRANSFER_BIT;
                    barrier.srcAccessMask = 0;
                    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                    cmd->ImageBarrier(image, subResourceRange, barrier, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
                }

                uint32_t width  = std::max(imageDesc.extent.width >> request.mipLevel, 1U);
                uint32_t height = std::max(imageDesc.extent.height >> request.mipLevel, 1U);

                uint32_t rowLength   = bufferBlockLengthFn(width, imageInfo->blockWidth);
                uint32_t imageHeight = bufferBlockLengthFn(height, imageInfo->blockHeight);

                static constexpr uint64_t STAGING_BLOCK_SIZE = 1 * 1024 * 1024;
                uint32_t                  blockNum           = rowLength * imageHeight;
                uint64_t                  srcSize            = blockNum * imageInfo->blockSize;
                uint64_t                  rowBlockSize       = rowLength * imageInfo->blockSize;
                uint32_t                  copyBlockHeight    = static_cast<uint32_t>(STAGING_BLOCK_SIZE / rowBlockSize);
                uint32_t                  copyNum            = (imageHeight + copyBlockHeight - 1) / copyBlockHeight;

                uint64_t bufferStep = copyBlockHeight * rowBlockSize;
                uint32_t heightStep = copyBlockHeight * imageInfo->blockHeight;

                for (uint32_t i = 0; i < copyNum; ++i) {
                    drv::Buffer::Descriptor bufferDesc = {};
                    bufferDesc.size                    = STAGING_BLOCK_SIZE;
                    bufferDesc.usage                   = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
                    bufferDesc.memory                  = VMA_MEMORY_USAGE_CPU_ONLY;
                    auto stagingBuffer                 = device->CreateDeviceObject<drv::Buffer>(bufferDesc);
                    stagingBuffers.emplace_back(stagingBuffer);

                    auto     ptr      = stagingBuffer->Map();
                    uint64_t copySize = std::min(bufferStep, srcSize - bufferStep * i);
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
        drawLinear.instanceCount = 13;

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
