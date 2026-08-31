//
// Created by blues on 2026/3/29.
//

#pragma once

#include <VulkanFunctions.h>
#include <aurora/rhi/CommandBuffer.h>
#include <memory>
#include <vector>

namespace sky::aurora {

    class VulkanDevice;

    class VulkanCommandBuffer : public CommandBuffer {
    public:
        VulkanCommandBuffer(VulkanDevice &device, VkCommandPool pool, VkCommandBuffer cmdBuffer);
        ~VulkanCommandBuffer() override;

        void Begin() override;
        void End() override;
        void PipelineBarrier(const BarrierInfo &info) override;

        std::unique_ptr<GraphicsEncoder> CreateGraphicsEncoder() override;
        std::unique_ptr<ComputeEncoder>  CreateComputeEncoder() override;
        std::unique_ptr<BlitEncoder>     CreateBlitEncoder() override;

        VkCommandBuffer GetNativeHandle() const
        {
            return cmdBuffer;
        }

    private:
        VulkanDevice   &device;
        VkCommandPool   pool      = VK_NULL_HANDLE;
        VkCommandBuffer cmdBuffer = VK_NULL_HANDLE;
    };

    class VulkanCommandPool : public CommandPool {
    public:
        VulkanCommandPool(VulkanDevice &device, uint32_t queueFamilyIndex, VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);
        ~VulkanCommandPool() override;

        bool           Init() override;
        void           Reset() override;
        CommandBuffer *Allocate() override;

        VkCommandPool GetNativeHandle() const
        {
            return pool;
        }

    private:
        VulkanDevice        &device;
        uint32_t             queueFamilyIndex;
        VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        VkCommandPool        pool  = VK_NULL_HANDLE;

        std::vector<VulkanCommandBuffer *> allocatedBuffers;
    };

} // namespace sky::aurora
