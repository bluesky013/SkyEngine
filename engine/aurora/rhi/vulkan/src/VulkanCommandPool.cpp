//
// Created by blues on 2026/3/29.
//

#include <VulkanCommandPool.h>
#include <VulkanDevice.h>
#include <VulkanEncoder.h>
#include <core/logger/Logger.h>

static const char *TAG = "AuroraVulkan";

namespace sky::aurora {

    // ---- VulkanCommandBuffer ----

    VulkanCommandBuffer::VulkanCommandBuffer(VulkanDevice &device, VkCommandPool pool, VkCommandBuffer cmdBuffer)
        : device(device)
        , pool(pool)
        , cmdBuffer(cmdBuffer)
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
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        device.GetDeviceFn().vkBeginCommandBuffer(cmdBuffer, &beginInfo);
    }

    void VulkanCommandBuffer::End()
    {
        device.GetDeviceFn().vkEndCommandBuffer(cmdBuffer);
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

    VulkanCommandPool::VulkanCommandPool(VulkanDevice &device, uint32_t queueFamilyIndex)
        : device(device)
        , queueFamilyIndex(queueFamilyIndex)
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
        poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = queueFamilyIndex;
        poolInfo.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        VkResult result = device.GetDeviceFn().vkCreateCommandPool(device.GetNativeHandle(), &poolInfo, nullptr, &pool);
        if (result != VK_SUCCESS) {
            LOG_E(TAG, "failed to create VkCommandPool, error: %d", result);
            return false;
        }
        return true;
    }

    void VulkanCommandPool::Reset()
    {
        device.GetDeviceFn().vkResetCommandPool(device.GetNativeHandle(), pool, 0);
    }

    CommandBuffer *VulkanCommandPool::Allocate()
    {
        VkCommandBufferAllocateInfo allocInfo = {};
        allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool        = pool;
        allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer vkCmdBuffer = VK_NULL_HANDLE;
        VkResult result = device.GetDeviceFn().vkAllocateCommandBuffers(device.GetNativeHandle(), &allocInfo, &vkCmdBuffer);
        if (result != VK_SUCCESS) {
            LOG_E(TAG, "failed to allocate VkCommandBuffer, error: %d", result);
            return nullptr;
        }

        auto *cmdBuffer = new VulkanCommandBuffer(device, pool, vkCmdBuffer);
        allocatedBuffers.push_back(cmdBuffer);
        return cmdBuffer;
    }

} // namespace sky::aurora

