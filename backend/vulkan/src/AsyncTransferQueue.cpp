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

        {
            std::unique_lock<std::mutex> lock(mutex);
            lastTaskId.store(task.taskId);
        }
        taskCv.notify_all();
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
            bufferInfo.size               = BLOCK_SIZE * INFLIGHT_NUM;
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

            const uint8_t *src = request.source->GetData(request.offset);
            for (uint32_t i = 0; i < copyNum; ++i) {
                VkBufferCopy copy = {};
                copy.size         = std::min(BLOCK_SIZE, size);
                copy.srcOffset    = BeginFrame();
                copy.dstOffset    = dstOffset;

                memcpy(mapped + copy.srcOffset, src + copy.size, copy.size);

                inflightCommands[currentFrameId]->Copy(stagingBuffer, buffer, copy);

                EndFrame();

                dstOffset += BLOCK_SIZE;
                size -= BLOCK_SIZE;
            }
        });
    }

    TransferTaskHandle AsyncTransferQueue::UploadImage(const ImagePtr &image, const std::vector<rhi::ImageUploadRequest> &requests)
    {
        return CreateTask([this, image, requests]() {
            auto *uploadQueue = GetQueue();
            auto &imageInfo = image->GetImageInfo();
            auto &formatInfo  = image->GetFormatInfo();

            inflightCommands[currentFrameId]->Wait();
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
            inflightCommands[currentFrameId]->QueueBarrier(image, subResourceRange, barrier, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
            inflightCommands[currentFrameId]->FlushBarriers();
            inflightCommands[currentFrameId]->End();
            inflightCommands[currentFrameId]->Submit(*queue, {});
            inflightCommands[currentFrameId]->Wait();

            for (auto &request : requests) {
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
                    memcpy(mapped + offset, request.data + request.offset + bufferStep * i, copySize);

                    VkOffset3D offset3D = {request.imageOffset.x, request.imageOffset.y, request.imageOffset.z};
                    offset3D.y += static_cast<int32_t>(heightStep * i);

                    VkBufferImageCopy copy = {};
                    copy.bufferOffset      = offset;
                    copy.bufferRowLength   = rowLength * formatInfo.blockWidth;
                    copy.bufferImageHeight = imageHeight * formatInfo.blockHeight;
                    copy.imageSubresource  = {VK_IMAGE_ASPECT_COLOR_BIT, request.mipLevel, 0, 1};
                    copy.imageOffset       = offset3D;
                    copy.imageExtent       = {width, std::min(height - heightStep * i, heightStep), 1};

                    inflightCommands[currentFrameId]->Copy(stagingBuffer, image, copy);

                    EndFrame();
                }
            }

            inflightCommands[currentFrameId]->Wait();
            inflightCommands[currentFrameId]->Begin();
            barrier.srcStageMask  = VK_PIPELINE_STAGE_TRANSFER_BIT;
            barrier.dstStageMask  = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = 0;
            inflightCommands[currentFrameId]->QueueBarrier(image, subResourceRange, barrier, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            inflightCommands[currentFrameId]->FlushBarriers();
            inflightCommands[currentFrameId]->End();
            inflightCommands[currentFrameId]->Submit(*queue, {});
            inflightCommands[currentFrameId]->Wait();
        });
    }

    TransferTaskHandle AsyncTransferQueue::UploadImage(const ImagePtr &image, const rhi::ImageUploadRequest &request)
    {
        return CreateTask([this, image, request]() {
            auto *uploadQueue = GetQueue();
            auto &imageDesc   = image->GetImageInfo();
            auto &formatInfo  = image->GetFormatInfo();

            VkImageSubresourceRange subResourceRange = {};
            subResourceRange.aspectMask              = VK_IMAGE_ASPECT_COLOR_BIT;
            subResourceRange.baseMipLevel            = request.mipLevel;
            subResourceRange.levelCount              = 1;
            subResourceRange.baseArrayLayer          = request.layer;
            subResourceRange.layerCount              = 1;
            vk::Barrier barrier                      = {};
            {
                BeginFrame();
                barrier.srcStageMask  = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                barrier.dstStageMask  = VK_PIPELINE_STAGE_TRANSFER_BIT;
                barrier.srcAccessMask = 0;
                barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                inflightCommands[currentFrameId]->ImageBarrier(image, subResourceRange, barrier, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
                EndFrame();
            }

            uint32_t width  = std::max(imageDesc.extent.width >> request.mipLevel, 1U);
            uint32_t height = std::max(imageDesc.extent.height >> request.mipLevel, 1U);

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
                memcpy(mapped + offset, request.data + request.offset + bufferStep * i, copySize);

                VkBufferImageCopy copy = {};
                copy.bufferOffset      = offset;
                copy.bufferRowLength   = rowLength * formatInfo.blockWidth;
                copy.bufferImageHeight = imageHeight * formatInfo.blockHeight;
                copy.imageSubresource  = {VK_IMAGE_ASPECT_COLOR_BIT, request.mipLevel, 0, 1};
                copy.imageOffset       = {0, static_cast<int32_t>(heightStep * i), 0};
                copy.imageExtent       = {width, std::min(height - heightStep * i, heightStep), 1};

                inflightCommands[currentFrameId]->Copy(stagingBuffer, image, copy);

                EndFrame();
            }

            {
                BeginFrame();
                barrier.srcStageMask  = VK_PIPELINE_STAGE_TRANSFER_BIT;
                barrier.dstStageMask  = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                barrier.dstAccessMask = 0;
                inflightCommands[currentFrameId]->ImageBarrier(image, subResourceRange, barrier, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                EndFrame();
            }
        });
    }

    void AsyncTransferQueue::Wait(TransferTaskHandle handle)
    {
        std::unique_lock<std::mutex> lock(mutex);
        taskCv.wait(lock, [&]() {return HasComplete(handle); });
    }
}
