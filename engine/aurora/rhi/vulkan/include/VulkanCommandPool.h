//
// Created by blues on 2026/3/29.
//

#pragma once

#include <VulkanFunctions.h>
#include <vector>

namespace sky::aurora {

    class VulkanDevice;

    class VulkanCommandBuffer {
    public:
        VulkanCommandBuffer(const VulkanDeviceFunctions &fn, VkDevice device, VkCommandPool pool, VkCommandBuffer cmdBuffer);
        ~VulkanCommandBuffer();

        void Begin();
        void End();

        VkCommandBuffer GetNativeHandle() const { return cmdBuffer; }

    private:
        const VulkanDeviceFunctions &fn;
        VkDevice        device    = VK_NULL_HANDLE;
        VkCommandPool   pool      = VK_NULL_HANDLE;
        VkCommandBuffer cmdBuffer = VK_NULL_HANDLE;
    };

    class VulkanCommandPool {
    public:
        VulkanCommandPool(VulkanDevice &device, uint32_t queueFamilyIndex);
        ~VulkanCommandPool();

        bool Init();

        void Reset();
        VulkanCommandBuffer *Allocate(VkCommandBufferLevel level);

        VkCommandPool GetNativeHandle() const { return pool; }

    private:
        VulkanDevice &device;
        uint32_t      queueFamilyIndex;
        VkCommandPool pool = VK_NULL_HANDLE;

        std::vector<VulkanCommandBuffer*> allocatedBuffers;
    };

} // namespace sky::aurora

