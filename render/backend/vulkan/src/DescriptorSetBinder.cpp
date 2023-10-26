//
// Created by Zach Lee on 2022/8/1.
//

#include <vulkan/DescriptorSetBinder.h>
#include <vulkan/Util.h>

namespace sky::vk {

    static uint32_t CombineSlotBinding(uint32_t set, uint32_t binding)
    {
        return ((set & 0xFFFF) << 16) | (binding & 0xFFFF);
    }

    void DescriptorSetBinder::SetPipelineLayout(const PipelineLayoutPtr &layout)
    {
        pipelineLayout = layout;

        auto layoutNum  = layout->GetSetNumber();
        auto dynamicNum = layout->GetDynamicNum();
        sets.resize(layoutNum);
        vkSets.resize(layoutNum);
        dynamicOffsets.resize(dynamicNum, 0);

        const auto &desLayouts = layout->GetLayouts();
        uint32_t    offset     = 0;
        for (uint32_t i = 0; i < layoutNum; ++i) {
            const auto &bindings = desLayouts[i]->GetDescriptorTable();
            for (const auto &[binding, info] : bindings) {
                if (!IsDynamicDescriptor(info.descriptorType)) {
                    continue;
                }
                offsetIndex[CombineSlotBinding(i, binding)] = offset;
                offset += info.descriptorCount;
            }
        }
    }

    void DescriptorSetBinder::SetBindPoint(VkPipelineBindPoint bp)
    {
        bindPoint = bp;
    }

    void DescriptorSetBinder::OnBind(VkCommandBuffer cmd) const
    {
        if (!sets.empty() && pipelineLayout) {
            vkCmdBindDescriptorSets(cmd, bindPoint, pipelineLayout->GetNativeHandle(), 0, static_cast<uint32_t>(vkSets.size()), vkSets.data(),
                                    static_cast<uint32_t>(dynamicOffsets.size()), dynamicOffsets.data());
        }
    }

    void DescriptorSetBinder::BindSet(uint32_t slot, const DescriptorSetPtr &set)
    {
        if (slot >= sets.size()) {
            return;
        }
        sets[slot]   = set;
        vkSets[slot] = set->GetNativeHandle();
    }

    void DescriptorSetBinder::SetOffset(uint32_t set, uint32_t binding, uint32_t index, uint32_t offset)
    {
        uint32_t idx = CombineSlotBinding(set, index);
        auto iter = offsetIndex.find(idx);
        if (iter != offsetIndex.end() && iter->second < dynamicOffsets.size()) {
            dynamicOffsets[iter->second + index] = offset;
        }
    }

} // namespace sky::vk
