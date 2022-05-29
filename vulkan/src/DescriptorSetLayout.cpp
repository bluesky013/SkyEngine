//
// Created by Zach Lee on 2022/1/9.
//

#include <vulkan/DescriptorSetLayout.h>
#include <vulkan/Device.h>
#include <core/hash/Hash.h>
#include <core/hash/Crc32.h>
#include <list>

namespace sky::drv {

    DescriptorSetLayout::DescriptorSetLayout(Device& dev) : DevObject(dev), layout(VK_NULL_HANDLE), hash(0)
    {

    }

    bool DescriptorSetLayout::Init(const Descriptor& des)
    {
        descriptor = des;

        std::vector<VkDescriptorSetLayoutBinding> bindings;
        std::list<VkSampler> samplers;

        for (auto& binding : des.bindings) {
            HashCombine32(hash, Crc32::Cal(binding.first));
            HashCombine32(hash, Crc32::Cal(binding.second));

            VkDescriptorSetLayoutBinding layoutBinding = {};
            layoutBinding.binding            = binding.first;
            layoutBinding.descriptorType     = binding.second.descriptorType;
            layoutBinding.descriptorCount    = binding.second.descriptorCount;
            layoutBinding.stageFlags         = binding.second.stageFlags;
            layoutBinding.pImmutableSamplers = nullptr;
            bindings.emplace_back(std::move(layoutBinding));
        }

        VkDescriptorSetLayoutCreateInfo layoutInfo = {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();
        layout = device.GetDescriptorSetLayout(hash, &layoutInfo);
        if (layout == VK_NULL_HANDLE) {
            return false;
        }

        return true;
    }

    VkDescriptorSetLayout DescriptorSetLayout::GetNativeHandle() const
    {
        return layout;
    }

    uint32_t DescriptorSetLayout::GetHash() const
    {
        return hash;
    }

    const std::map<uint32_t, DescriptorSetLayout::SetBinding>& DescriptorSetLayout::GetDescriptorTable() const
    {
        return descriptor.bindings;
    }
}