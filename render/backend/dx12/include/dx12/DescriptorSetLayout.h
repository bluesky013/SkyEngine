//
// Created by Zach Lee on 2022/11/10.
//

#pragma once

#include <rhi/DescriptorSetLayout.h>
#include <dx12/DevObject.h>
#include <vector>

namespace sky::dx {

    class DescriptorSetLayout : public rhi::DescriptorSetLayout, public DevObject {
    public:
        explicit DescriptorSetLayout(Device &dev);
        ~DescriptorSetLayout() override = default;

        const std::unordered_map<rhi::ShaderStageFlags, D3D12_ROOT_DESCRIPTOR_TABLE> &GetDescriptorTableMap() const { return descriptorTableMap; }

    private:
        friend class Device;
        bool Init(const Descriptor &desc);

        std::unordered_map<rhi::ShaderStageFlags, std::vector<D3D12_DESCRIPTOR_RANGE>> rangeMap;
        std::unordered_map<rhi::ShaderStageFlags, D3D12_ROOT_DESCRIPTOR_TABLE> descriptorTableMap;
    };

}