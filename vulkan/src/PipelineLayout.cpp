//
// Created by Zach Lee on 2022/1/9.
//

#include <vulkan/PipelineLayout.h>
#include <vulkan/Device.h>
#include <core/hash/Crc32.h>
#include <core/hash/Hash.h>

namespace sky::drv {

    PipelineLayout::PipelineLayout(Device& dev) : DevObject(dev), layout(VK_NULL_HANDLE), hash(0)
    {

    }

    PipelineLayout::~PipelineLayout()
    {
    }

    bool PipelineLayout::Init(const Descriptor& des)
    {
        std::vector<VkDescriptorSetLayout> layouts;
        for (auto& desSet : des.desLayouts) {
            auto layout = device.CreateDeviceObject<DescriptorSetLayout>(desSet.second);
            HashCombine32(hash, layout->GetHash());
            desLayouts.emplace(desSet.first, layout);
            layouts.emplace_back(layout->GetNativeHandle());
        }

        for (auto& push : des.pushConstants) {
            HashCombine32(hash, Crc32::Cal(push));
        }

        VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
        pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutCreateInfo.setLayoutCount = static_cast<uint32_t>(layouts.size());
        pipelineLayoutCreateInfo.pSetLayouts = layouts.data();
        pipelineLayoutCreateInfo.pushConstantRangeCount = static_cast<uint32_t>(des.pushConstants.size());
        pipelineLayoutCreateInfo.pPushConstantRanges = des.pushConstants.data();

        layout = device.GetPipelineLayout(hash, &pipelineLayoutCreateInfo);
        if (layout == VK_NULL_HANDLE) {
            return false;
        }
        return true;
    }

    VkPipelineLayout PipelineLayout::GetNativeHandle() const
    {
        return layout;
    }

    uint32_t PipelineLayout::GetHash() const
    {
        return hash;
    }

    DescriptorSetPtr PipelineLayout::Allocate(DescriptorSetPool& pool, uint32_t slot)
    {
        auto iter = desLayouts.find(slot);
        if (iter == desLayouts.end()) {
            return {};
        }
        return pool.Allocate(iter->second);
    }
}