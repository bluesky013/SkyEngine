//
// Created by Zach Lee on 2022/1/3.
//

#include <vulkan/Device.h>
#include <vulkan/Queue.h>
#include <core/math/MathUtil.h>
#include <core/profile/Profiler.h>

namespace sky::vk {
    CommandBufferPtr Queue::AllocateCommandBuffer(const CommandBuffer::VkDescriptor &des)
    {
        return pool->Allocate(des);
    }

    CommandBufferPtr Queue::AllocateTlsCommandBuffer(const CommandBuffer::VkDescriptor &des)
    {
        const auto &tlsPool = GetOrCreatePool();
        return tlsPool->Allocate(des);
    }

    const CommandPoolPtr &Queue::GetOrCreatePool()
    {
        std::lock_guard<std::mutex> lock(mutex);
        auto &tmpPool = tlsPools[std::this_thread::get_id()];
        if (!tmpPool) {
            tmpPool = device.CreateDeviceObject<CommandPool>(CommandPool::VkDescriptor{});
        }
        return tmpPool;
    }

    void Queue::WaitIdle()
    {
        vkQueueWaitIdle(queue);
    }

    void Queue::SetupInternal()
    {
        CreateTask([this]() {
            CommandPool::VkDescriptor des = {};
            des.queueFamilyIndex        = queueFamilyIndex;
            des.flag                    = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

            pool = device.CreateDeviceObject<CommandPool>(des);

            Fence::VkDescriptor fenceInfo = {};
            fenceInfo.flag = VK_FENCE_CREATE_SIGNALED_BIT;

            for (uint32_t i = 0; i < INFLIGHT_NUM; ++i) {
                stagingBuffers[i] = std::make_unique<rhi::StagingBufferPool>();
                stagingBuffers[i]->Init(device, BUFFER_PER_FRAME_SIZE);

                fences[i] = std::static_pointer_cast<Fence>(device.CreateFence({}));
                inflightCommands[i] = AllocateCommandBuffer({});
            }

        });
    }

    void Queue::PostShutdown()
    {
    }

    void Queue::BeginFrame()
    {
        fences[currentFrameId]->WaitAndReset();
        inflightCommands[currentFrameId]->Begin();
        stagingBuffers[currentFrameId]->Reset();
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
            SKY_PROFILE_NAME("Queue Upload Image")

            auto vkImage = std::static_pointer_cast<Image>(image);
            const auto &imageInfo = vkImage->GetImageInfo();
            const auto &formatInfo  = vkImage->GetFormatInfo();

            BeginFrame();

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

            auto *stagingBuffer = stagingBuffers[currentFrameId].get();
            for (const auto &request : requests) {
                uint32_t width  = std::max(request.imageExtent.width >> request.mipLevel, 1U);
                uint32_t height = std::max(request.imageExtent.height >> request.mipLevel, 1U);
                uint32_t depth = std::max(request.imageExtent.depth >> request.mipLevel, 1U);

                uint32_t rowLength   = Ceil(width, formatInfo.blockWidth);
                uint32_t imageHeight = Ceil(height, formatInfo.blockHeight);

                uint32_t blockNum  = rowLength * imageHeight;
                uint32_t sliceSize = blockNum * formatInfo.blockSize;
                uint32_t rowBlockSize = rowLength * formatInfo.blockSize;
                uint32_t layerSize = sliceSize * depth;

                uint32_t heightPerFrame = BUFFER_PER_FRAME_SIZE / rowBlockSize;
                uint32_t slicePerFrame = BUFFER_PER_FRAME_SIZE / sliceSize;

                uint32_t sliceNum    = slicePerFrame == 0 ? depth : Ceil(depth, slicePerFrame);
                uint32_t rowBlockNum = slicePerFrame == 0 ? Ceil(imageHeight, heightPerFrame) : 1;

                uint64_t bufferStep = slicePerFrame == 0 ? heightPerFrame * rowBlockSize : slicePerFrame * sliceSize;
                uint32_t heightStep = slicePerFrame == 0 ? heightPerFrame * formatInfo.blockHeight : height;
                uint32_t sliceStep = std::max(slicePerFrame, 1U);


                for (uint32_t i = 0; i < sliceNum; ++i) {
                    VkOffset3D offset3D = {request.imageOffset.x, request.imageOffset.y,
                                           request.imageOffset.z + static_cast<int32_t>(sliceStep * i)};

                    for (uint32_t j = 0; j < rowBlockNum; ++j) {
                        uint64_t copySize = std::min(bufferStep, layerSize - bufferStep * j);

                        auto view = stagingBuffers[currentFrameId]->Allocate(copySize, 4);
                        if (view.ptr == nullptr) {
                            EndFrame();
                            BeginFrame();

                            // try again;
                            stagingBuffer = stagingBuffers[currentFrameId].get();
                            view = stagingBuffer->Allocate(copySize, 4);
                        }

                        request.source->ReadData(request.offset + sliceSize * i + bufferStep * j, copySize, view.ptr);

                        VkBufferImageCopy copy = {};
                        copy.bufferOffset      = view.offset;
                        copy.bufferRowLength   = rowLength * formatInfo.blockWidth;
                        copy.bufferImageHeight = imageHeight * formatInfo.blockHeight;
                        copy.imageSubresource  = {VK_IMAGE_ASPECT_COLOR_BIT, request.mipLevel, request.layer, 1};
                        copy.imageOffset       = offset3D;
                        copy.imageExtent       = {width,
                                                  std::min(height - heightStep * j, heightStep),
                                                  std::min(depth - sliceStep * i, sliceStep)};

                        inflightCommands[currentFrameId]->Copy(std::static_pointer_cast<Buffer>(stagingBuffer->GetBuffer()), vkImage, copy);
                        offset3D.y += static_cast<int32_t>(heightStep);
                    }
                }
            }

            barrier.srcStageMask  = VK_PIPELINE_STAGE_TRANSFER_BIT;
            barrier.dstStageMask  = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = 0;
            inflightCommands[currentFrameId]->QueueBarrier(vkImage, subResourceRange, barrier, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            inflightCommands[currentFrameId]->FlushBarriers();
            EndFrame();
            vkQueueWaitIdle(queue);
        });
    }

    rhi::TransferTaskHandle Queue::UploadBuffer(const rhi::BufferPtr &buffer, const std::vector<rhi::BufferUploadRequest> &requests)
    {
        return CreateTask([this, buffer, requests]() {
            SKY_PROFILE_NAME("Queue Upload Buffer")

            auto vkBuffer = std::static_pointer_cast<Buffer>(buffer);

            BeginFrame();
            auto *stagingBuffer = stagingBuffers[currentFrameId].get();
            for (const auto &request : requests) {
                auto copyNum = static_cast<uint32_t>(std::ceil(static_cast<double>(request.size) / static_cast<double>(BUFFER_PER_FRAME_SIZE)));
                uint64_t size = request.size;
                uint64_t dstOffset = 0;

                for (uint32_t i = 0; i < copyNum; ++i) {
                    VkBufferCopy copy = {};
                    copy.size = std::min(BUFFER_PER_FRAME_SIZE, size);

                    auto view = stagingBuffers[currentFrameId]->Allocate(copy.size, 4);
                    if (view.ptr == nullptr) {
                        EndFrame();
                        BeginFrame();

                        // try again;
                        stagingBuffer = stagingBuffers[currentFrameId].get();
                        view = stagingBuffer->Allocate(copy.size, 4);
                    }

                    copy.srcOffset = view.offset;
                    copy.dstOffset = dstOffset;

                    request.source->ReadData(request.offset + dstOffset, copy.size, view.ptr);

                    inflightCommands[currentFrameId]->Copy(std::static_pointer_cast<Buffer>(stagingBuffer->GetBuffer()), vkBuffer, copy);
                    dstOffset += BUFFER_PER_FRAME_SIZE;
                    size = std::max(size, BUFFER_PER_FRAME_SIZE) - BUFFER_PER_FRAME_SIZE;
                }
            }
            EndFrame();
            vkQueueWaitIdle(queue);
        });
    }

//    rhi::TransferTaskHandle Queue::UploadImage(const rhi::ImagePtr &image, const std::vector<rhi::ImageUploadRequest> &requests)
//    {
//        return CreateTask([this, image, requests]() {
//            auto vkImage = std::static_pointer_cast<Image>(image);
//            const auto &imageInfo = vkImage->GetImageInfo();
//            const auto &formatInfo  = vkImage->GetFormatInfo();
//
//            BeginFrame();
//            VkImageSubresourceRange subResourceRange = {};
//            subResourceRange.aspectMask              = VK_IMAGE_ASPECT_COLOR_BIT;
//            subResourceRange.baseMipLevel            = 0;
//            subResourceRange.levelCount              = imageInfo.mipLevels;
//            subResourceRange.baseArrayLayer          = 0;
//            subResourceRange.layerCount              = imageInfo.arrayLayers;
//            vk::Barrier barrier                      = {};
//            barrier.srcStageMask  = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
//            barrier.dstStageMask  = VK_PIPELINE_STAGE_TRANSFER_BIT;
//            barrier.srcAccessMask = 0;
//            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
//            inflightCommands[currentFrameId]->QueueBarrier(vkImage, subResourceRange, barrier, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
//            inflightCommands[currentFrameId]->FlushBarriers();
//            EndFrame();
//
//            for (const auto &request : requests) {
//                uint32_t width  = std::max(request.imageExtent.width >> request.mipLevel, 1U);
//                uint32_t height = std::max(request.imageExtent.height >> request.mipLevel, 1U);
//                uint32_t depth = std::max(request.imageExtent.depth >> request.mipLevel, 1U);
//
//                uint32_t rowLength   = Ceil(width, formatInfo.blockWidth);
//                uint32_t imageHeight = Ceil(height, formatInfo.blockHeight);
//
//                uint32_t blockNum        = rowLength * imageHeight;
//                uint32_t sliceSize       = blockNum * formatInfo.blockSize;
//                uint32_t imageSize       = sliceSize * depth;
//
//                uint32_t rowBlockSize    = rowLength * formatInfo.blockSize;
//                uint32_t copyBlockHeight = BUFFER_PER_FRAME_SIZE / rowBlockSize;
//                uint32_t copyDepth       = BUFFER_PER_FRAME_SIZE / sliceSize;
//
//                uint32_t copyNum         = Ceil(imageHeight, copyBlockHeight);
//
//                uint32_t bufferStep = copyBlockHeight * rowBlockSize;
//                uint32_t heightStep = copyBlockHeight * formatInfo.blockHeight;
//
//
//                for (uint32_t i = 0; i < copyNum; ++i) {
//                    uint64_t offset = BeginFrame();
//
//                    uint32_t copySize = std::min(bufferStep, imageSize - bufferStep * i);
//                    request.source->ReadData(request.offset + bufferStep * i, copySize, mapped + offset);
//
//                    VkOffset3D offset3D = {request.imageOffset.x, request.imageOffset.y, request.imageOffset.z};
//                    offset3D.y += static_cast<int32_t>(heightStep * i);
//
//                    VkBufferImageCopy copy = {};
//                    copy.bufferOffset      = offset;
//                    copy.bufferRowLength   = rowLength * formatInfo.blockWidth;
//                    copy.bufferImageHeight = imageHeight * formatInfo.blockHeight;
//                    copy.imageSubresource  = {VK_IMAGE_ASPECT_COLOR_BIT, request.mipLevel, request.layer, 1};
//                    copy.imageOffset       = offset3D;
//                    copy.imageExtent       = {width, std::min(height - heightStep * i, heightStep), 1};
//
//                    inflightCommands[currentFrameId]->Copy(stagingBuffer, vkImage, copy);
//                    EndFrame();
//                }
//            }
//
//            BeginFrame();
//            barrier.srcStageMask  = VK_PIPELINE_STAGE_TRANSFER_BIT;
//            barrier.dstStageMask  = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
//            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
//            barrier.dstAccessMask = 0;
//            inflightCommands[currentFrameId]->QueueBarrier(vkImage, subResourceRange, barrier, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
//            inflightCommands[currentFrameId]->FlushBarriers();
//            EndFrame();
//        });
//    }
//
//    rhi::TransferTaskHandle Queue::UploadBuffer(const rhi::BufferPtr &buffer, const std::vector<rhi::BufferUploadRequest> &requests)
//    {
//        return CreateTask([this, buffer, requests]() {
//            auto vkBuffer = std::static_pointer_cast<Buffer>(buffer);
//
//            for (const auto &request : requests) {
//                auto copyNum = static_cast<uint32_t>(std::ceil(request.size / static_cast<double>(BUFFER_PER_FRAME_SIZE)));
//                uint64_t size = request.size;
//                uint64_t dstOffset = 0;
//
//                for (uint32_t i = 0; i < copyNum; ++i) {
//                    VkBufferCopy copy = {};
//                    copy.size = std::min(BUFFER_PER_FRAME_SIZE, size);
//                    copy.srcOffset = BeginFrame();
//                    copy.dstOffset = dstOffset;
//
//                    request.source->ReadData(dstOffset, copy.size, mapped + copy.srcOffset);
//
//                    inflightCommands[currentFrameId]->Copy(stagingBuffer, vkBuffer, copy);
//
//                    EndFrame();
//
//                    dstOffset += BUFFER_PER_FRAME_SIZE;
//                    size = std::max(size, BUFFER_PER_FRAME_SIZE) - BUFFER_PER_FRAME_SIZE;
//                }
//            }
//        });
//    }
} // namespace sky::vk
