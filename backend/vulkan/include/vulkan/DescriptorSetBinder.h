//
// Created by Zach Lee on 2022/8/1.
//

#pragma once

#include <vector>
#include <vulkan/PipelineLayout.h>

namespace sky::vk {

    class DescriptorSetBinder {
    public:
        DescriptorSetBinder()  = default;
        ~DescriptorSetBinder() = default;

        void SetPipelineLayout(const PipelineLayoutPtr &layout);

        void SetBindPoint(VkPipelineBindPoint bindPoint);

        void OnBind(VkCommandBuffer) const;

        void BindSet(uint32_t slot, const DescriptorSetPtr &set);

        void SetOffset(uint32_t slot, uint32_t binding, uint32_t offset);

    private:
        PipelineLayoutPtr   pipelineLayout;
        VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

        std::vector<DescriptorSetPtr>          sets;
        std::vector<VkDescriptorSet>           vkSets;
        std::vector<uint32_t>                  dynamicOffsets;
        std::unordered_map<uint32_t, uint32_t> offsetIndex;
    };
    using DescriptorSetBinderPtr = std::shared_ptr<DescriptorSetBinder>;

} // namespace sky::vk
