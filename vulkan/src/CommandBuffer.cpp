//
// Created by Zach Lee on 2021/11/7.
//

#include "vulkan/CommandBuffer.h"
#include "vulkan/Device.h"
#include "vulkan/Fence.h"
#include "vulkan/Semaphore.h"
#include "core/logger/Logger.h"

static const char* TAG = "Driver";

namespace sky::drv {

    CommandBuffer::CommandBuffer(Device& dev, VkCommandPool cp, VkCommandBuffer cb)
        : DevObject(dev), pool(cp), cmdBuffer(cb), fence()
    {
    }

    CommandBuffer::~CommandBuffer()
    {
        Wait();

        if (pool != VK_NULL_HANDLE && cmdBuffer != VK_NULL_HANDLE) {
            vkFreeCommandBuffers(device.GetNativeHandle(), pool, 1, &cmdBuffer);
        }
    }

    bool CommandBuffer::Init(const Descriptor& des)
    {
        if (des.needFence) {
            Fence::Descriptor fenceDes = {};
            fenceDes.flag = VK_FENCE_CREATE_SIGNALED_BIT;
            fence = device.CreateDeviceObject<Fence>(fenceDes);
            if (!fence) {
                return false;
            }
        }
        
        return true;
    }

    void CommandBuffer::Wait(uint64_t timeout)
    {
        if (fence) {
            fence->Wait(timeout);
        }
    }

    void CommandBuffer::Begin()
    {
        if (fence) {
            fence->Reset();
        }

        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmdBuffer, &beginInfo);
    }

    void CommandBuffer::End()
    {
        vkEndCommandBuffer(cmdBuffer);
    }

    void CommandBuffer::Submit(Queue& queue, const SubmitInfo& submit)
    {
        uint32_t waitSize = (uint32_t)submit.waits.size();
        std::vector<VkPipelineStageFlags> waitStages(waitSize);
        std::vector<VkSemaphore> waitSemaphores(waitSize);
        for (uint32_t i = 0; i < waitSize; ++i) {
            waitStages[i] = submit.waits[i].first;
            waitSemaphores[i] = submit.waits[i].second->GetNativeHandle();
        }
        
        uint32_t signalSize = (uint32_t)submit.submitSignals.size();
        std::vector<VkSemaphore> signalSemaphores(signalSize);
        for (uint32_t i = 0; i < signalSize; ++i) {
            signalSemaphores[i] = submit.submitSignals[i]->GetNativeHandle();
        }

        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmdBuffer;

        if (!submit.submitSignals.empty()) {
            submitInfo.signalSemaphoreCount = signalSize;
            submitInfo.pSignalSemaphores = signalSemaphores.data();
        }

        if (!waitStages.empty()) {
            submitInfo.waitSemaphoreCount = waitSize;
            submitInfo.pWaitDstStageMask = waitStages.data();
            submitInfo.pWaitSemaphores = waitSemaphores.data();
        }
        vkQueueSubmit(queue.GetNativeHandle(), 1, &submitInfo, fence ? fence->GetNativeHandle() : VK_NULL_HANDLE);
    }

    VkCommandBuffer CommandBuffer::GetNativeHandle() const
    {
        return cmdBuffer;
    }

    GraphicsEncoder CommandBuffer::EncodeGraphics()
    {
        GraphicsEncoder encoder;
        encoder.cmdBuffer = cmdBuffer;
        return encoder;
    }

    void CommandBuffer::Copy(BufferPtr src, ImagePtr dst, const VkBufferImageCopy& copy)
    {
        vkCmdCopyBufferToImage(cmdBuffer, src->GetNativeHandle(), dst->GetNativeHandle(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
    }

    void CommandBuffer::ImageBarrier(ImagePtr image, const VkImageSubresourceRange& subresourceRange, const Barrier& barrier, VkImageLayout src, VkImageLayout dst)
    {
        VkImageMemoryBarrier imageMemoryBarrier = {};
        imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        imageMemoryBarrier.srcAccessMask = barrier.srcAccessMask;
        imageMemoryBarrier.dstAccessMask = barrier.dstAccessMask;
        imageMemoryBarrier.image = image->GetNativeHandle();
        imageMemoryBarrier.subresourceRange = subresourceRange;
        imageMemoryBarrier.oldLayout = src;
        imageMemoryBarrier.newLayout = dst;
        imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

        vkCmdPipelineBarrier(cmdBuffer, barrier.srcStageMask, barrier.dstStageMask,
            0,
            0, nullptr,
            0, nullptr,
            1, &imageMemoryBarrier);
    }

    void CommandBuffer::Copy(VkImage src, VkImageLayout srcLayout,
        VkImage dst, VkImageLayout dstLayout, const VkImageCopy& copy)
    {
        vkCmdCopyImage(cmdBuffer, src, srcLayout, dst, dstLayout, 1, &copy);
    }

    void CommandBuffer::Copy(BufferPtr src, BufferPtr dst, const VkBufferCopy& copy)
    {
        vkCmdCopyBuffer(cmdBuffer, src->GetNativeHandle(), dst->GetNativeHandle(), 1, &copy);
    }

    GraphicsEncoder::GraphicsEncoder()
    {
        vkBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        vkBeginInfo.pNext = nullptr;
        vkBeginInfo.renderPass = VK_NULL_HANDLE;
        vkBeginInfo.framebuffer = VK_NULL_HANDLE;
        vkBeginInfo.renderArea = {};
        vkBeginInfo.clearValueCount = 0;
        vkBeginInfo.pClearValues = nullptr;
    }

    void GraphicsEncoder::BeginPass(const PassBeginInfo& beginInfo)
    {
        vkBeginInfo.renderPass = beginInfo.renderPass->GetNativeHandle();
        auto& extent = beginInfo.frameBuffer->GetExtent();

        if (beginInfo.renderArea != nullptr) {
            vkBeginInfo.renderArea = *beginInfo.renderArea;
        } else {
            vkBeginInfo.framebuffer = beginInfo.frameBuffer->GetNativeHandle();
            vkBeginInfo.renderArea.offset.x = 0;
            vkBeginInfo.renderArea.offset.y = 0;
            vkBeginInfo.renderArea.extent = extent;
        }

        vkBeginInfo.clearValueCount = beginInfo.clearValueCount;
        vkBeginInfo.pClearValues = beginInfo.clearValues;

        vkCmdBeginRenderPass(cmdBuffer, &vkBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport viewport = {0, 0, static_cast<float>(extent.width), static_cast<float>(extent.height), 0.f, 1.f};
        VkRect2D scissor = {{0, 0}, extent};

        vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);
        vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);
    }

    void GraphicsEncoder::EndPass()
    {
        vkCmdEndRenderPass(cmdBuffer);
    }

    void GraphicsEncoder::Encode(const DrawItem& item)
    {
        if (!item.pso) {
            return;
        }
        vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, item.pso->GetNativeHandle());
        if (item.viewport != nullptr && item.viewportCount != 0) {
            vkCmdSetViewport(cmdBuffer, 0, item.viewportCount, item.viewport);
        }

        if (item.scissor != nullptr && item.scissor != 0) {
            vkCmdSetScissor(cmdBuffer, 0, item.scissorCount, item.scissor);
        }

        if (item.descriptorSets != nullptr && item.descriptorSetCount != 0) {
            std::vector<VkDescriptorSet> sets(item.descriptorSetCount);
            for (uint8_t i = 0; i < item.descriptorSetCount; ++i) {
                sets[i] = item.descriptorSets[i]->GetNativeHandle();
            }
            vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, item.pso->GetPipelineLayout()->GetNativeHandle(),
                0, item.descriptorSetCount, sets.data(), 0, nullptr);
        }

        if (item.vertexBuffer != nullptr && item.vertexBufferCount != 0) {
            std::vector<VkBuffer> buffers(item.vertexBufferCount);
            std::vector<VkDeviceSize> offsets(item.vertexBufferCount);
            for (uint8_t i = 0; i < item.vertexBufferCount; ++i) {
                buffers[i] = item.vertexBuffer[i].buffer->GetNativeHandle();
                offsets[i] = item.vertexBuffer[i].offset;
            }

            vkCmdBindVertexBuffers(cmdBuffer, 0, item.vertexBufferCount, buffers.data(), offsets.data());
        }

        if (item.indexBuffer != nullptr) {
            vkCmdBindIndexBuffer(cmdBuffer, item.indexBuffer->buffer->GetNativeHandle(), item.indexBuffer->offset, VK_INDEX_TYPE_UINT32);
        }
        
        if (item.drawArgs != nullptr) {
            switch (item.drawArgs->type) {
                case CmdDrawType::INDEXED:
                    vkCmdDrawIndexed(cmdBuffer, item.drawArgs->indexed.indexCount, item.drawArgs->indexed.instanceCount,
                        item.drawArgs->indexed.firstIndex, item.drawArgs->indexed.vertexOffset, item.drawArgs->indexed.firstInstance);
                    break;
                case CmdDrawType::LINEAR:
                    vkCmdDraw(cmdBuffer, item.drawArgs->linear.vertexCount,
                        item.drawArgs->linear.instanceCount, item.drawArgs->linear.firstVertex, item.drawArgs->linear.firstInstance);
                    break;
            }
            
        }

    }
}