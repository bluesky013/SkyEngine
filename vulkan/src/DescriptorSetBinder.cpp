//
// Created by Zach Lee on 2022/8/1.
//

#include <vulkan/DescriptorSetBinder.h>

namespace sky::drv {

    void DescriptorSetBinder::SetPipelineLayout(const PipelineLayoutPtr& layout)
    {
        pipelineLayout = layout;

        auto layoutNum = layout->GetSlotNumber();
        auto dynamicNum = layout->GetDynamicNum();
        sets.resize(layoutNum);
        vkSets.resize(layoutNum);
        offsetIndex.resize(layoutNum, dynamicNum);
        dynamicOffsets.resize(dynamicNum, 0);

        auto& desLayouts = layout->GetLayouts();
        uint32_t offset = 0;
        for (uint32_t i = 0; i < layoutNum; ++i) {
            auto num = desLayouts[i]->GetDynamicNum();
            if (num != 0) {
                offsetIndex[i] = offset;
                offset += num;
            }
        }
    }

    void DescriptorSetBinder::SetBindPoint(VkPipelineBindPoint bp)
    {
        bindPoint = bp;
    }

    void DescriptorSetBinder::OnBind(VkCommandBuffer cmd)
    {
        if (!sets.empty() && pipelineLayout) {
            vkCmdBindDescriptorSets(cmd, bindPoint, pipelineLayout->GetNativeHandle(), 0,
                                    static_cast<uint32_t>(vkSets.size()), vkSets.data(),
                                    static_cast<uint32_t>(dynamicOffsets.size()), dynamicOffsets.data());
        }
    }

    void DescriptorSetBinder::BindSet(uint32_t slot, const DescriptorSetPtr& set)
    {
        if (slot >= sets.size()) {
            return;
        }
        sets[slot] = set;
        vkSets[slot] = set->GetNativeHandle();
    }

    void DescriptorSetBinder::SetOffset(uint32_t slot, uint32_t index, uint32_t offset)
    {
        uint32_t idx = offsetIndex[slot] + index;
        if (idx < dynamicOffsets.size()) {
            dynamicOffsets[idx] = offset;
        }
    }

}