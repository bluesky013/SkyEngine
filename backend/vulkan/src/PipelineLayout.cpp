//
// Created by Zach Lee on 2022/1/9.
//

#include <core/hash/Crc32.h>
#include <core/hash/Hash.h>
#include <vulkan/Device.h>
#include <vulkan/PipelineLayout.h>
#include <vulkan/PushConstants.h>

namespace sky::vk {

    PipelineLayout::PipelineLayout(Device &dev) : DevObject(dev), layout(VK_NULL_HANDLE), dynamicNum(0), hash(0)
    {
    }

    PipelineLayout::~PipelineLayout()
    {
    }

    bool PipelineLayout::Init(const Descriptor &des)
    {
        hash = 0;
        std::vector<VkDescriptorSetLayout> layouts;
        for (auto &layout : des.layouts) {
            desLayouts.emplace_back(std::static_pointer_cast<DescriptorSetLayout>(layout));
            layouts.emplace_back(desLayouts.back()->GetNativeHandle());
            HashCombine32(hash, desLayouts.back()->GetHash());
        }

        VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
        pipelineLayoutCreateInfo.sType                      = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutCreateInfo.setLayoutCount             = static_cast<uint32_t>(layouts.size());
        pipelineLayoutCreateInfo.pSetLayouts                = layouts.data();
        pipelineLayoutCreateInfo.pushConstantRangeCount     = 0;
        pipelineLayoutCreateInfo.pPushConstantRanges        = nullptr;

        layout = device.GetPipelineLayout(hash, &pipelineLayoutCreateInfo);
        if (layout == VK_NULL_HANDLE) {
            return false;
        }
        return true;
    }

    bool PipelineLayout::Init(const VkDescriptor &des)
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

    DescriptorSetLayoutPtr PipelineLayout::GetLayout(uint32_t slot) const
    {
        if (slot >= desLayouts.size()) {
            return {};
        }
        return desLayouts[slot];
    }

    rhi::DescriptorSetLayoutPtr PipelineLayout::GetSetLayout(uint32_t set) const
    {
        return std::static_pointer_cast<DescriptorSetLayout>(GetLayout(set));
    }

    DescriptorSetPtr PipelineLayout::Allocate(DescriptorSetPoolPtr pool, uint32_t slot)
    {
        if (slot >= desLayouts.size()) {
            return {};
        }
        return DescriptorSet::Allocate(pool, desLayouts[slot]);
    }
} // namespace sky::vk
