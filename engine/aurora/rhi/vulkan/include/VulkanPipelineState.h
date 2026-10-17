//
// Created by Zach Lee on 2026/4/1.
//

#pragma once

#include <aurora/rhi/PipelineState.h>
#include <vulkan/vulkan.h>

namespace sky::aurora {

    class VulkanDevice;

    // Render-pass compatibility is selected before graphics pipeline creation.
    struct SubpassInfo {
        VkRenderPass renderPass    = VK_NULL_HANDLE;
        uint32_t     subpass       = 0;
        uint64_t     cacheIdentity = 0;
    };

    class VulkanGraphicsPipeline : public GraphicsPipeline {
    public:
        explicit VulkanGraphicsPipeline(VulkanDevice &dev);
        ~VulkanGraphicsPipeline() override;

        // Dynamic rendering path (Vulkan 1.3+ / VK_KHR_dynamic_rendering)
        bool Init(const Descriptor &desc);

        // Legacy render pass path
        bool Init(const Descriptor &desc, const SubpassInfo &subpassInfo);

        VkPipeline GetNativeHandle() const
        {
            return pipeline;
        }
        VkPipelineLayout GetLayoutHandle() const
        {
            return layoutHandle;
        }
        VkResult GetCreateResult() const noexcept
        {
            return createResult;
        }

    private:
        bool BuildPipeline(const Descriptor &desc, const void *pNext, const SubpassInfo *subpassInfo);

        VulkanDevice    &device;
        VkPipeline       pipeline     = VK_NULL_HANDLE;
        VkPipelineLayout layoutHandle = VK_NULL_HANDLE;
        VkResult         createResult = VK_SUCCESS;
    };

    class VulkanComputePipeline : public ComputePipeline {
    public:
        explicit VulkanComputePipeline(VulkanDevice &dev);
        ~VulkanComputePipeline() override;

        bool Init(const Descriptor &desc);

        VkPipeline GetNativeHandle() const
        {
            return pipeline;
        }
        VkPipelineLayout GetLayoutHandle() const
        {
            return layoutHandle;
        }

    private:
        VulkanDevice    &device;
        VkPipeline       pipeline     = VK_NULL_HANDLE;
        VkPipelineLayout layoutHandle = VK_NULL_HANDLE;
    };

} // namespace sky::aurora
