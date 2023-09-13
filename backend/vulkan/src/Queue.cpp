//
// Created by Zach Lee on 2022/1/3.
//

#include <vulkan/Device.h>
#include <vulkan/Queue.h>
#include <core/math/MathUtil.h>

namespace sky::vk {
    CommandBufferPtr Queue::AllocateCommandBuffer(const CommandBuffer::VkDescriptor &des)
    {
        return pool->Allocate(des);
    }

    CommandBufferPtr Queue::AllocateTlsCommandBuffer(const CommandBuffer::VkDescriptor &des)
    {
        auto &tlsPool = GetOrCreatePool();
        return tlsPool->Allocate(des);
    }

    const CommandPoolPtr &Queue::GetOrCreatePool()
    {
        std::lock_guard<std::mutex> lock(mutex);
        auto                       &pool = tlsPools[std::this_thread::get_id()];
        if (!pool) {
            pool = device.CreateDeviceObject<CommandPool>(CommandPool::VkDescriptor{});
        }
        return pool;
    }

    void Queue::WaitIdle()
    {
        vkQueueWaitIdle(queue);
    }

    void Queue::SetupInternal()
    {
        CommandPool::VkDescriptor des = {};
        des.queueFamilyIndex        = queueFamilyIndex;
        des.flag                    = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        pool = device.CreateDeviceObject<CommandPool>(des);

        for (auto & fence : fences) {
            fence = std::static_pointer_cast<Fence>(device.CreateFence({}));
        }

        CreateTask([this]() {
            Buffer::VkDescriptor bufferInfo = {};
            bufferInfo.size               = BLOCK_SIZE * INFLIGHT_NUM;
            bufferInfo.usage              = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            bufferInfo.memory             = VMA_MEMORY_USAGE_CPU_ONLY;

            stagingBuffer = device.CreateDeviceObject<vk::Buffer>(bufferInfo);
            mapped        = stagingBuffer->Map();

            Fence::VkDescriptor fenceInfo = {};
            fenceInfo.flag = VK_FENCE_CREATE_SIGNALED_BIT;

            for (auto & inflightCommand : inflightCommands) {
                inflightCommand = AllocateCommandBuffer({});
            }
        });
    }

    void Queue::PostShutdown()
    {
        stagingBuffer->UnMap();
        stagingBuffer = nullptr;
    }

    uint64_t Queue::BeginFrame()
    {
        fences[currentFrameId]->WaitAndReset();
        inflightCommands[currentFrameId]->Begin();
        return currentFrameId * BLOCK_SIZE;
    }

    void Queue::EndFrame()
    {
        inflightCommands[currentFrameId]->End();
        inflightCommands[currentFrameId]->Submit(*this, {{}, {}, {fences[currentFrameId]}});
        currentFrameId = (currentFrameId + 1) % INFLIGHT_NUM;
    }

    rhi::TransferTaskHandle Queue::UploadImage(const rhi::ImagePtr &image, const std::vector<rhi::ImageUploadRequest> &requests)
    {
        return CreateTask([this, image, requests]() {
            auto vkImage = std::static_pointer_cast<Image>(image);
            const auto &imageInfo = vkImage->GetImageInfo();
            const auto &formatInfo  = vkImage->GetFormatInfo();

            fences[currentFrameId]->Reset();
            inflightCommands[currentFrameId]->Begin();
            VkImageSubresourceRange subResourceRange = {};
            subResourceRange.aspectMask              = VK_IMAGE_ASPECT_COLOR_BIT;
            subResourceRange.baseMipLevel            = 0;
            subResourceRange.levelCount              = imageInfo.mipLevels;
            subResourceRange.baseArrayLayer          = 0;
            subResourceRange.layerCount              = imageInfo.arrayLayers;
            vk::Barrier barrier                      = {};
            barrier.srcStageMask  = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            barrier.dstStageMask  = VK_PIPELINE_STAGE_TRANSFER_BIT;
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            inflightCommands[currentFrameId]->QueueBarrier(vkImage, subResourceRange, barrier, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
            inflightCommands[currentFrameId]->FlushBarriers();

            inflightCommands[currentFrameId]->End();
            inflightCommands[currentFrameId]->Submit(*this, {{}, {}, fences[currentFrameId]});
            fences[currentFrameId]->Wait();

            for (const auto &request : requests) {
                uint32_t width  = std::max(request.imageExtent.width >> request.mipLevel, 1U);
                uint32_t height = std::max(request.imageExtent.height >> request.mipLevel, 1U);

                uint32_t rowLength   = Ceil(width, formatInfo.blockWidth);
                uint32_t imageHeight = Ceil(height, formatInfo.blockHeight);

                uint32_t blockNum        = rowLength * imageHeight;
                uint32_t srcSize         = blockNum * formatInfo.blockSize;
                uint32_t rowBlockSize    = rowLength * formatInfo.blockSize;
                uint32_t copyBlockHeight = BLOCK_SIZE / rowBlockSize;
                uint32_t copyNum         = Ceil(imageHeight, copyBlockHeight);

                uint32_t bufferStep = copyBlockHeight * rowBlockSize;
                uint32_t heightStep = copyBlockHeight * formatInfo.blockHeight;

                for (uint32_t i = 0; i < copyNum; ++i) {
                    uint64_t offset = BeginFrame();

                    uint32_t copySize = std::min(bufferStep, srcSize - bufferStep * i);
                    request.source->ReadData(request.offset + bufferStep * i, copySize, mapped + offset);
//                    memcpy(mapped + offset, request.data + request.offset + bufferStep * i, copySize);

                    VkOffset3D offset3D = {request.imageOffset.x, request.imageOffset.y, request.imageOffset.z};
                    offset3D.y += static_cast<int32_t>(heightStep * i);

                    VkBufferImageCopy copy = {};
                    copy.bufferOffset      = offset;
                    copy.bufferRowLength   = rowLength * formatInfo.blockWidth;
                    copy.bufferImageHeight = imageHeight * formatInfo.blockHeight;
                    copy.imageSubresource  = {VK_IMAGE_ASPECT_COLOR_BIT, request.mipLevel, request.layer, 1};
                    copy.imageOffset       = offset3D;
                    copy.imageExtent       = {width, std::min(height - heightStep * i, heightStep), 1};

                    inflightCommands[currentFrameId]->Copy(stagingBuffer, vkImage, copy);
                    EndFrame();
                }
            }

            fences[currentFrameId]->Reset();
            inflightCommands[currentFrameId]->Begin();
            barrier.srcStageMask  = VK_PIPELINE_STAGE_TRANSFER_BIT;
            barrier.dstStageMask  = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = 0;

            inflightCommands[currentFrameId]->QueueBarrier(vkImage, subResourceRange, barrier, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            inflightCommands[currentFrameId]->FlushBarriers();

            inflightCommands[currentFrameId]->End();
            inflightCommands[currentFrameId]->Submit(*this, {{}, {}, fences[currentFrameId]});
            fences[currentFrameId]->Wait();
        });
    }

    rhi::TransferTaskHandle Queue::UploadBuffer(const rhi::BufferPtr &buffer, const std::vector<rhi::BufferUploadRequest> &requests)
    {
        return CreateTask([this, buffer, requests]() {
            auto vkBuffer = std::static_pointer_cast<Buffer>(buffer);

            for (auto &request : requests) {
                auto copyNum = static_cast<uint32_t>(std::ceil(request.size / static_cast<double>(BLOCK_SIZE)));
                uint64_t size = request.size;
                uint64_t dstOffset = 0;

                const uint8_t *src = request.source->GetData(request.offset);
                for (uint32_t i = 0; i < copyNum; ++i) {
                    VkBufferCopy copy = {};
                    copy.size = std::min(BLOCK_SIZE, size);
                    copy.srcOffset = BeginFrame();
                    copy.dstOffset = dstOffset;

                    memcpy(mapped + copy.srcOffset, src + dstOffset, copy.size);

                    inflightCommands[currentFrameId]->Copy(stagingBuffer, vkBuffer, copy);

                    EndFrame();

                    dstOffset += BLOCK_SIZE;
                    size -= BLOCK_SIZE;
                }
            }
        });
    }
} // namespace sky::vk
