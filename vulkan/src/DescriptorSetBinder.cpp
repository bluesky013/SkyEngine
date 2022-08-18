//
// Created by Zach Lee on 2022/8/1.
//

#include <vulkan/DescriptorSetBinder.h>

namespace sky::drv {

    void DescriptorSetBinder::SetPipelineLayout(PipelineLayoutPtr layout)
    {
        pipelineLayout = layout;

        auto layoutNum = layout->GetSlotNumber();
        sets.resize(layoutNum);
        vkSets.resize(layoutNum);
        dynamicOffsets.resize(layout->GetDynamicNum());
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

    void DescriptorSetBinder::BindSet(uint32_t slot, DescriptorSetPtr set)
    {
        if (slot >= sets.size()) {
            return;
        }
        sets[slot] = set;
        vkSets[slot] = set->GetNativeHandle();
    }

}