//
// Created by Zach Lee on 2022/1/9.
//

#include <core/hash/Crc32.h>
#include <core/hash/Hash.h>
#include <vulkan/Device.h>
#include <vulkan/PipelineLayout.h>
#include <vulkan/PushConstants.h>

namespace sky::drv {

    PipelineLayout::PipelineLayout(Device &dev) : DevObject(dev), layout(VK_NULL_HANDLE), dynamicNum(0), hash(0)
    {
    }

    PipelineLayout::~PipelineLayout()
    {
    }

    bool PipelineLayout::Init(const Descriptor &des)
    {
        desLayouts.resize(des.desLayouts.size());
        pushConstants = des.pushConstants;
        std::vector<VkDescriptorSetLayout> layouts(des.desLayouts.size());
        for (uint32_t i = 0; i < des.desLayouts.size(); ++i) {
            auto layout = device.CreateDeviceObject<DescriptorSetLayout>(des.desLayouts[i]);
            HashCombine32(hash, layout->GetHash());
            desLayouts[i] = layout;
            layouts[i]    = layout->GetNativeHandle();
            dynamicNum += layout->GetDynamicNum();
        }

        for (auto &push : des.pushConstants) {
            HashCombine32(hash, Crc32::Cal(push));
        }

        VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
        pipelineLayoutCreateInfo.sType                      = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutCreateInfo.setLayoutCount             = static_cast<uint32_t>(layouts.size());
        pipelineLayoutCreateInfo.pSetLayouts                = layouts.data();
        pipelineLayoutCreateInfo.pushConstantRangeCount     = static_cast<uint32_t>(des.pushConstants.size());
        pipelineLayoutCreateInfo.pPushConstantRanges        = des.pushConstants.data();

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

    uint32_t PipelineLayout::GetSlotNumber() const
    {
        return static_cast<uint32_t>(desLayouts.size());
    }

    uint32_t PipelineLayout::GetDynamicNum() const
    {
        return dynamicNum;
    }

    DescriptorSetLayoutPtr PipelineLayout::GetLayout(uint32_t slot) const
    {
        if (slot >= desLayouts.size()) {
            return {};
        }
        return desLayouts[slot];
    }

    const std::vector<DescriptorSetLayoutPtr> &PipelineLayout::GetLayouts() const
    {
        return desLayouts;
    }

    const std::vector<VkPushConstantRange> &PipelineLayout::GetConstantRanges() const
    {
        return pushConstants;
    }

    DescriptorSetPtr PipelineLayout::Allocate(DescriptorSetPoolPtr pool, uint32_t slot)
    {
        if (slot >= desLayouts.size()) {
            return {};
        }
        return DescriptorSet::Allocate(pool, desLayouts[slot]);
    }
} // namespace sky::drv