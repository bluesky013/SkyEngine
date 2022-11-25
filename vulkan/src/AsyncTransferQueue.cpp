//
// Created by Zach Lee on 2022/10/19.
//

#include <algorithm>
#include <vulkan/AsyncTransferQueue.h>
#include <vulkan/Device.h>
#include <vulkan/Queue.h>
#include <core/math/MathUtil.h>
#include <rhi/Decode.h>

namespace sky::vk {

    bool AsyncTransferQueue::HasTask()
    {
        std::lock_guard<std::mutex> lock(taskMutex);
        return !taskQueue.empty();
    }

    bool AsyncTransferQueue::EmitSingleTask()
    {
        TransferTask task;
        {
            std::lock_guard<std::mutex> lock(taskMutex);
            if (taskQueue.empty()) {
                return false;
            }
            task = std::move(taskQueue.front());
            taskQueue.pop_front();
        }

        task.func();
        lastTaskId.store(task.taskId);
        return true;
    }

    void AsyncTransferQueue::ThreadMain()
    {
        while (!exit.load()) {
            if (!EmitSingleTask()) {
                std::unique_lock<std::mutex> lock(mutex);
                while (!exit.load() && !HasTask()) {
                    cv.wait(lock);
                }
            }
        }
    }

    void AsyncTransferQueue::Setup()
    {
        thread = std::thread(&AsyncTransferQueue::ThreadMain, this);

        CreateTask([this]() {
            Buffer::VkDescriptor bufferInfo = {};
            bufferInfo.size               = 16;
            bufferInfo.usage              = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            bufferInfo.memory             = VMA_MEMORY_USAGE_CPU_ONLY;

            stagingBuffer = device.CreateDeviceObject<vk::Buffer>(bufferInfo);
            mapped        = stagingBuffer->Map();

            for (uint32_t i = 0; i < INFLIGHT_NUM; ++i) {
                inflightCommands[i] = queue->AllocateCommandBuffer({});
            }
        });
    }

    void AsyncTransferQueue::Shutdown()
    {
        exit.store(true);
        {
            std::lock_guard<std::mutex> lock(mutex);
            cv.notify_all();
        }
        if (thread.joinable()) {
            thread.join();
        }
        stagingBuffer->UnMap();
        stagingBuffer = nullptr;
    }

    uint64_t AsyncTransferQueue::BeginFrame()
    {
        inflightCommands[currentFrameId]->Wait();
        inflightCommands[currentFrameId]->Begin();
        return currentFrameId * BLOCK_SIZE;
    }

    void AsyncTransferQueue::EndFrame()
    {
        inflightCommands[currentFrameId]->End();
        inflightCommands[currentFrameId]->Submit(*queue, {});
        currentFrameId = (currentFrameId + 1) % INFLIGHT_NUM;
    }

    TransferTaskHandle AsyncTransferQueue::UploadBuffer(const BufferPtr &buffer, const rhi::BufferUploadRequest &request)
    {
        return CreateTask([this, buffer, request]() {
            uint32_t copyNum   = static_cast<uint32_t>(std::ceil(request.size / static_cast<double>(BLOCK_SIZE)));
            uint64_t size      = request.size;
            uint64_t dstOffset = 0;

            const uint8_t *src = request.data + request.offset;
            for (uint32_t i = 0; i < copyNum; ++i) {
                VkBufferCopy copy = {};
                copy.size         = std::min(BLOCK_SIZE, size);
                copy.srcOffset    = BeginFrame();
                copy.dstOffset    = dstOffset;

                uint64_t offset = BeginFrame();
                memcpy(mapped + offset, src + copy.size, copy.size);

                inflightCommands[currentFrameId]->Copy(stagingBuffer, buffer, copy);

                EndFrame();

                dstOffset += BLOCK_SIZE;
                size -= BLOCK_SIZE;
            }
        });
    }

    TransferTaskHandle AsyncTransferQueue::UploadImage(const ImagePtr &image, const rhi::ImageUploadRequest &request)
    {
        return CreateTask([this, image, request]() {
            std::vector<vk::BufferPtr> stagingBuffers;

            auto *uploadQueue = GetQueue();
            auto &imageDesc   = image->GetImageInfo();
            auto &formatInfo  = image->GetFormatInfo();

            auto cmd = uploadQueue->AllocateCommandBuffer({});
            cmd->Begin();

            VkImageSubresourceRange subResourceRange = {};
            subResourceRange.aspectMask              = VK_IMAGE_ASPECT_COLOR_BIT;
            subResourceRange.baseMipLevel            = request.mipLevel;
            subResourceRange.levelCount              = 1;
            subResourceRange.baseArrayLayer          = 0;
            subResourceRange.layerCount              = 1;
            vk::Barrier barrier                      = {};
            {
                barrier.srcStageMask  = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                barrier.dstStageMask  = VK_PIPELINE_STAGE_TRANSFER_BIT;
                barrier.srcAccessMask = 0;
                barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                cmd->ImageBarrier(image, subResourceRange, barrier, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
            }

            uint32_t width  = std::max(imageDesc.extent.width >> request.mipLevel, 1U);
            uint32_t height = std::max(imageDesc.extent.height >> request.mipLevel, 1U);

            uint32_t rowLength   = Ceil(width, formatInfo.blockWidth);
            uint32_t imageHeight = Ceil(height, formatInfo.blockHeight);

            static constexpr uint32_t STAGING_BLOCK_SIZE = 1 * 1024 * 1024;
            uint32_t                  blockNum           = rowLength * imageHeight;
            uint32_t                  srcSize            = blockNum * formatInfo.blockSize;
            uint32_t                  rowBlockSize       = rowLength * formatInfo.blockSize;
            uint32_t                  copyBlockHeight    = STAGING_BLOCK_SIZE / rowBlockSize;
            uint32_t                  copyNum            = Ceil(imageHeight, copyBlockHeight);

            uint32_t bufferStep = copyBlockHeight * rowBlockSize;
            uint32_t heightStep = copyBlockHeight * formatInfo.blockHeight;

            for (uint32_t i = 0; i < copyNum; ++i) {
                vk::Buffer::VkDescriptor bufferDesc = {};
                bufferDesc.size                   = STAGING_BLOCK_SIZE;
                bufferDesc.usage                  = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
                bufferDesc.memory                 = VMA_MEMORY_USAGE_CPU_ONLY;
                auto stagingBuffer                = device.CreateDeviceObject<vk::Buffer>(bufferDesc);
                stagingBuffers.emplace_back(stagingBuffer);

                auto     ptr      = stagingBuffer->Map();
                uint32_t copySize = std::min(bufferStep, srcSize - bufferStep * i);
                memcpy(ptr, request.data + request.offset + bufferStep * i, copySize);
                stagingBuffer->UnMap();

                VkBufferImageCopy copy = {};
                copy.bufferOffset      = 0;
                copy.bufferRowLength   = rowLength * formatInfo.blockWidth;
                copy.bufferImageHeight = imageHeight * formatInfo.blockHeight;
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
                cmd->ImageBarrier(image, subResourceRange, barrier, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            }

            cmd->End();
            cmd->Submit(*uploadQueue, {});
            cmd->Wait();
        });
    }

}
