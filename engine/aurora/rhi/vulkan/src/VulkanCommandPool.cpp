//
// Created by blues on 2026/3/29.
//

#include <VulkanBuffer.h>
#include <VulkanCommandPool.h>
#include <VulkanConversion.h>
#include <VulkanDevice.h>
#include <VulkanEncoder.h>
#include <VulkanImage.h>
#include <core/logger/Logger.h>
#include <vector>

static const char *TAG = "AuroraVulkan";

namespace sky::aurora {

    // ---- VulkanCommandBuffer ----

    VulkanCommandBuffer::VulkanCommandBuffer(VulkanDevice &device, VkCommandPool pool, VkCommandBuffer cmdBuffer)
        : device(device), pool(pool), cmdBuffer(cmdBuffer)
    {
    }

    VulkanCommandBuffer::~VulkanCommandBuffer()
    {
        if (cmdBuffer != VK_NULL_HANDLE) {
            const auto &fn = device.GetDeviceFn();
            fn.vkFreeCommandBuffers(device.GetNativeHandle(), pool, 1, &cmdBuffer);
        }
    }

    void VulkanCommandBuffer::Begin()
    {
        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType                    = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags                    = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        device.GetDeviceFn().vkBeginCommandBuffer(cmdBuffer, &beginInfo);
    }

    void VulkanCommandBuffer::End()
    {
        device.GetDeviceFn().vkEndCommandBuffer(cmdBuffer);
    }

    void VulkanCommandBuffer::PipelineBarrier(const BarrierInfo &info)
    {
        const auto &fn = device.GetDeviceFn();

        const VkPipelineStageFlags2 srcStage = FromPipelineStageFlags2(info.srcStage);
        const VkPipelineStageFlags2 dstStage = FromPipelineStageFlags2(info.dstStage);

        std::vector<VkMemoryBarrier2> mems;
        mems.reserve(info.memoryBarriers.size());
        for (const auto &m : info.memoryBarriers) {
            VkMemoryBarrier2 b = {};
            b.sType            = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
            b.srcStageMask     = srcStage;
            b.srcAccessMask    = FromAccessFlags2(m.srcAccess);
            b.dstStageMask     = dstStage;
            b.dstAccessMask    = FromAccessFlags2(m.dstAccess);
            mems.push_back(b);
        }

        std::vector<VkBufferMemoryBarrier2> bufs;
        bufs.reserve(info.bufferBarriers.size());
        for (const auto &bb : info.bufferBarriers) {
            VkBufferMemoryBarrier2 b = {};
            b.sType                  = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
            b.srcStageMask           = srcStage;
            b.srcAccessMask          = FromAccessFlags2(bb.srcAccess);
            b.dstStageMask           = dstStage;
            b.dstAccessMask          = FromAccessFlags2(bb.dstAccess);
            b.srcQueueFamilyIndex    = VK_QUEUE_FAMILY_IGNORED;
            b.dstQueueFamilyIndex    = VK_QUEUE_FAMILY_IGNORED;
            b.buffer                 = static_cast<VulkanBuffer *>(bb.buffer)->GetNativeHandle();
            b.offset                 = bb.offset;
            b.size                   = bb.range == 0 ? VK_WHOLE_SIZE : bb.range;
            bufs.push_back(b);
        }

        std::vector<VkImageMemoryBarrier2> imgs;
        imgs.reserve(info.imageBarriers.size());
        for (const auto &ib : info.imageBarriers) {
            auto                 *img = static_cast<VulkanImage *>(ib.image);
            VkImageMemoryBarrier2 b   = {};
            b.sType                   = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
            b.srcStageMask            = srcStage;
            b.srcAccessMask           = FromAccessFlags2(ib.srcAccess);
            b.dstStageMask            = dstStage;
            b.dstAccessMask           = FromAccessFlags2(ib.dstAccess);
            b.oldLayout               = FromImageLayout(ib.oldLayout);
            b.newLayout               = FromImageLayout(ib.newLayout);
            b.srcQueueFamilyIndex     = VK_QUEUE_FAMILY_IGNORED;
            b.dstQueueFamilyIndex     = VK_QUEUE_FAMILY_IGNORED;
            b.image                   = img->GetNativeHandle();
            b.subresourceRange.aspectMask =
                ib.subRange.aspectMask ? FromAspectFlags(ib.subRange.aspectMask) : InferAspectFromLayout(ib.newLayout, img->GetVkFormat());
            b.subresourceRange.baseMipLevel   = ib.subRange.baseLevel;
            b.subresourceRange.levelCount     = ib.subRange.levels;
            b.subresourceRange.baseArrayLayer = ib.subRange.baseLayer;
            b.subresourceRange.layerCount     = ib.subRange.layers;
            imgs.push_back(b);
        }

        VkDependencyInfo dep         = {};
        dep.sType                    = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        dep.memoryBarrierCount       = static_cast<uint32_t>(mems.size());
        dep.pMemoryBarriers          = mems.data();
        dep.bufferMemoryBarrierCount = static_cast<uint32_t>(bufs.size());
        dep.pBufferMemoryBarriers    = bufs.data();
        dep.imageMemoryBarrierCount  = static_cast<uint32_t>(imgs.size());
        dep.pImageMemoryBarriers     = imgs.data();

        fn.vkCmdPipelineBarrier2(cmdBuffer, &dep);
    }

    std::unique_ptr<GraphicsEncoder> VulkanCommandBuffer::CreateGraphicsEncoder()
    {
        return std::make_unique<VulkanGraphicsEncoder>(device, cmdBuffer);
    }

    std::unique_ptr<ComputeEncoder> VulkanCommandBuffer::CreateComputeEncoder()
    {
        return std::make_unique<VulkanComputeEncoder>(device, cmdBuffer);
    }

    std::unique_ptr<BlitEncoder> VulkanCommandBuffer::CreateBlitEncoder()
    {
        return std::make_unique<VulkanBlitEncoder>(device, cmdBuffer);
    }

    // ---- VulkanCommandPool ----

    VulkanCommandPool::VulkanCommandPool(VulkanDevice &device, uint32_t queueFamilyIndex, VkCommandBufferLevel level)
        : device(device), queueFamilyIndex(queueFamilyIndex), level(level)
    {
    }

    VulkanCommandPool::~VulkanCommandPool()
    {
        for (auto *buffer : allocatedBuffers) {
            delete buffer;
        }
        allocatedBuffers.clear();

        if (pool != VK_NULL_HANDLE) {
            device.GetDeviceFn().vkDestroyCommandPool(device.GetNativeHandle(), pool, nullptr);
        }
    }

    bool VulkanCommandPool::Init()
    {
        VkCommandPoolCreateInfo poolInfo = {};
        poolInfo.sType                   = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex        = queueFamilyIndex;
        poolInfo.flags                   = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        VkResult result = device.GetDeviceFn().vkCreateCommandPool(device.GetNativeHandle(), &poolInfo, nullptr, &pool);
        if (result != VK_SUCCESS) {
            LOG_E(TAG, "failed to create VkCommandPool, error: %d", result);
            return false;
        }
        return true;
    }

    void VulkanCommandPool::Reset()
    {
        if (pool != VK_NULL_HANDLE) {
            device.GetDeviceFn().vkResetCommandPool(device.GetNativeHandle(), pool, 0);
        }
    }

    CommandBuffer *VulkanCommandPool::Allocate()
    {
        VkCommandBufferAllocateInfo allocInfo = {};
        allocInfo.sType                       = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool                 = pool;
        allocInfo.level                       = level;
        allocInfo.commandBufferCount          = 1;

        VkCommandBuffer vkCmdBuffer = VK_NULL_HANDLE;
        VkResult        result      = device.GetDeviceFn().vkAllocateCommandBuffers(device.GetNativeHandle(), &allocInfo, &vkCmdBuffer);
        if (result != VK_SUCCESS) {
            LOG_E(TAG, "failed to allocate VkCommandBuffer, error: %d", result);
            return nullptr;
        }

        auto *cmdBuffer = new VulkanCommandBuffer(device, pool, vkCmdBuffer);
        allocatedBuffers.push_back(cmdBuffer);
        return cmdBuffer;
    }

} // namespace sky::aurora
