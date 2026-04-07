//
// Created by blues on 2026/3/29.
//

#pragma once

#include <aurora/rhi/CommandBuffer.h>
#include <VulkanFunctions.h>
#include <vector>
#include <memory>

namespace sky::aurora {

    class VulkanDevice;

    class VulkanCommandBuffer : public CommandBuffer {
    public:
        VulkanCommandBuffer(VulkanDevice &device, VkCommandPool pool, VkCommandBuffer cmdBuffer);
        ~VulkanCommandBuffer() override;

        void Begin() override;
        void End() override;

        std::unique_ptr<GraphicsEncoder> CreateGraphicsEncoder() override;
        std::unique_ptr<ComputeEncoder> CreateComputeEncoder() override;
        std::unique_ptr<BlitEncoder> CreateBlitEncoder() override;

        VkCommandBuffer GetNativeHandle() const { return cmdBuffer; }

    private:
        VulkanDevice   &device;
        VkCommandPool   pool      = VK_NULL_HANDLE;
        VkCommandBuffer cmdBuffer = VK_NULL_HANDLE;
    };

    class VulkanCommandPool : public CommandPool {
    public:
        VulkanCommandPool(VulkanDevice &device, uint32_t queueFamilyIndex);
        ~VulkanCommandPool() override;

        bool Init() override;
        void Reset() override;
        CommandBuffer *Allocate() override;

        VkCommandPool GetNativeHandle() const { return pool; }

    private:
        VulkanDevice &device;
        uint32_t      queueFamilyIndex;
        VkCommandPool pool = VK_NULL_HANDLE;

        std::vector<VulkanCommandBuffer*> allocatedBuffers;
    };

} // namespace sky::aurora

