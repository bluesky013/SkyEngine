//
// Created by Zach Lee on 2026/4/1.
//

#pragma once

#include <aurora/rhi/PipelineState.h>
#include <vulkan/vulkan.h>

namespace sky::aurora {

    class VulkanDevice;

    // Additional info needed when creating a VkGraphicsPipeline against a
    // traditional VkRenderPass (devices without VK_KHR_dynamic_rendering).
    struct SubpassInfo {
        VkRenderPass renderPass  = VK_NULL_HANDLE;
        uint32_t     subpass     = 0;
    };

    class VulkanGraphicsPipeline : public GraphicsPipeline {
    public:
        explicit VulkanGraphicsPipeline(VulkanDevice &dev);
        ~VulkanGraphicsPipeline() override;

        // Dynamic rendering path (Vulkan 1.3+ / VK_KHR_dynamic_rendering)
        bool Init(const Descriptor &desc);

        // Legacy render pass path
        bool Init(const Descriptor &desc, const SubpassInfo &subpassInfo);

        VkPipeline GetNativeHandle() const { return pipeline; }

    private:
        bool BuildPipeline(const Descriptor &desc, const void *pNext, const SubpassInfo *subpassInfo);

        VulkanDevice &device;
        VkPipeline    pipeline = VK_NULL_HANDLE;
    };

    class VulkanComputePipeline : public ComputePipeline {
    public:
        explicit VulkanComputePipeline(VulkanDevice &dev);
        ~VulkanComputePipeline() override;

        bool Init(const Descriptor &desc);

        VkPipeline GetNativeHandle() const { return pipeline; }

    private:
        VulkanDevice &device;
        VkPipeline    pipeline = VK_NULL_HANDLE;
    };

} // namespace sky::aurora