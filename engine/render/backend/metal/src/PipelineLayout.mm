//
// Created by Zach Lee on 2025/7/20.
//

#include <mtl/PipelineLayout.h>

namespace sky::mtl {

    rhi::DescriptorSetLayoutPtr PipelineLayout::GetSetLayout(uint32_t slot) const
    {
        if (slot >= desLayouts.size()) {
            return {};
        }
        return std::static_pointer_cast<DescriptorSetLayout>(desLayouts[slot]);
    }

    bool PipelineLayout::Init(const Descriptor &des)
    {
        for (const auto &desLayout : des.layouts) {
            desLayouts.emplace_back(std::static_pointer_cast<DescriptorSetLayout>(desLayout));
        }
        return true;
    }

} // namespace sky::mtl
