//
// Created by blues on 2026/3/29.
//

#pragma once

#include <aurora/rhi/CommandPool.h>
#include <vulkan/vulkan.h>
#include <vector>

namespace sky::aurora {

    class VulkanDevice;

    class VulkanCommandBuffer : public CommandBuffer {
    public:
        VulkanCommandBuffer(VkDevice device, VkCommandPool pool, VkCommandBuffer cmdBuffer);
        ~VulkanCommandBuffer() override;

        void Begin() override;
        void End() override;

        VkCommandBuffer GetNativeHandle() const { return cmdBuffer; }

    private:
        VkDevice        device    = VK_NULL_HANDLE;
        VkCommandPool   pool      = VK_NULL_HANDLE;
        VkCommandBuffer cmdBuffer = VK_NULL_HANDLE;
    };

    class VulkanCommandPool : public CommandPool {
    public:
        VulkanCommandPool(VulkanDevice &device, uint32_t queueFamilyIndex);
        ~VulkanCommandPool() override;

        bool Init();

        void Reset() override;
        CommandBuffer *Allocate(CommandBufferLevel level) override;

        VkCommandPool GetNativeHandle() const { return pool; }

    private:
        VulkanDevice &device;
        uint32_t      queueFamilyIndex;
        VkCommandPool pool = VK_NULL_HANDLE;

        std::vector<VulkanCommandBuffer*> allocatedBuffers;
    };

} // namespace sky::aurora

