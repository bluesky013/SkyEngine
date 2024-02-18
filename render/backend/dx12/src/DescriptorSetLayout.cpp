//
// Created by Zach Lee on 2022/11/10.
//

#include <dx12/DescriptorSetLayout.h>
#include <dx12/Device.h>
#include <dx12/Conversion.h>

namespace sky::dx {

    DescriptorSetLayout::DescriptorSetLayout(Device &dev) : DevObject(dev)
    {
    }


    bool DescriptorSetLayout::Init(const Descriptor &desc)
    {
        for (const auto &binding : desc.bindings) {
            auto &ranges = rangeMap[binding.visibility];
            ranges.emplace_back();
            auto &range = ranges.back();

            range.RangeType          = FromRHI(binding.type);
            range.NumDescriptors     = binding.count;
            range.BaseShaderRegister = binding.binding;
            range.RegisterSpace      = 0;
            // for unbounded size
            range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
        }

        for (const auto &[vis, ranges] : rangeMap) {
            descriptorTableMap[vis].pDescriptorRanges = ranges.data();
            descriptorTableMap[vis].NumDescriptorRanges = static_cast<uint32_t>(ranges.size());
        }

        return true;
    }
}