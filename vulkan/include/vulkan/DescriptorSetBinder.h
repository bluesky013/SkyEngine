//
// Created by Zach Lee on 2022/8/1.
//

#pragma once

#include <vector>
#include <vulkan/PipelineLayout.h>

namespace sky::drv {

    class DescriptorSetBinder {
    public:
        DescriptorSetBinder() = default;
        ~DescriptorSetBinder() = default;

        void SetPipelineLayout(PipelineLayoutPtr layout);

        void SetBindPoint(VkPipelineBindPoint bindPoint);

        void OnBind(VkCommandBuffer);

        void BindSet(uint32_t slot, DescriptorSetPtr set);
    private:
        PipelineLayoutPtr pipelineLayout;
        VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

        std::vector<DescriptorSetPtr> sets;
        std::vector<VkDescriptorSet> vkSets;
    };
    using DescriptorSetBinderPtr = std::shared_ptr<DescriptorSetBinder>;

}
