//
// Created by Zach Lee on 2022/11/10.
//

#include <dx12/DescriptorSetLayout.h>
#include <dx12/Device.h>
#include <dx12/Conversion.h>

namespace sky::dx {

    DescriptorSetLayout::DescriptorSetLayout(Device &dev) : DevObject(dev)
    {
        descriptorTable.NumDescriptorRanges = 0;
        descriptorTable.pDescriptorRanges = nullptr;
    }


    bool DescriptorSetLayout::Init(const Descriptor &desc)
    {
        ranges.resize(desc.bindings.size());
        for (uint32_t i = 0; i < ranges.size(); ++i) {
            auto &range = ranges[i];
            const auto &binding = desc.bindings[i];

            range.RangeType          = FromRHI(binding.type);
            range.NumDescriptors     = binding.count;
            range.BaseShaderRegister = binding.binding;
            range.RegisterSpace      = 0;

            // for unbounded size
            range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
        }

        return true;
    }
}