//
// Created by Zach Lee on 2022/1/9.
//

#include <core/hash/Crc32.h>
#include <core/hash/Hash.h>
#include <vulkan/Device.h>
#include <vulkan/PipelineLayout.h>
#include <vulkan/PushConstants.h>
#include <vulkan/Conversion.h>

namespace sky::vk {

    PipelineLayout::PipelineLayout(Device &dev) : DevObject(dev), layout(VK_NULL_HANDLE), dynamicNum(0), hash(0)
    {
    }

    bool PipelineLayout::Init(const Descriptor &des)
    {
        hash = 0;
        std::vector<VkDescriptorSetLayout> layouts;
        for (const auto &desLayout : des.layouts) {
            desLayouts.emplace_back(std::static_pointer_cast<DescriptorSetLayout>(desLayout));
            layouts.emplace_back(desLayouts.back()->GetNativeHandle());
            dynamicNum += desLayouts.back()->GetDynamicNum();
            HashCombine32(hash, desLayouts.back()->GetHash());
        }

        pushConstants.reserve(des.constants.size());
        for (const auto &push : des.constants) {
            auto &range = pushConstants.emplace_back();
            range.stageFlags = FromRHI(push.stageFlags);
            range.size = push.size;
            range.offset = push.offset;
            HashCombine32(hash, Crc32::Cal(push));
        }

        VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
        pipelineLayoutCreateInfo.sType                      = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutCreateInfo.setLayoutCount             = static_cast<uint32_t>(layouts.size());
        pipelineLayoutCreateInfo.pSetLayouts                = layouts.data();
        pipelineLayoutCreateInfo.pushConstantRangeCount     = static_cast<uint32_t>(pushConstants.size());
        pipelineLayoutCreateInfo.pPushConstantRanges        = pushConstants.data();

        layout = device.GetPipelineLayout(hash, &pipelineLayoutCreateInfo);
        return layout != VK_NULL_HANDLE;
    }

    bool PipelineLayout::Init(const VkDescriptor &des)
    {
        desLayouts.resize(des.desLayouts.size());
        pushConstants = des.pushConstants;
        std::vector<VkDescriptorSetLayout> layouts(des.desLayouts.size());
        for (uint32_t i = 0; i < des.desLayouts.size(); ++i) {
            auto desLayout = device.CreateDeviceObject<DescriptorSetLayout>(des.desLayouts[i]);
            HashCombine32(hash, desLayout->GetHash());
            desLayouts[i] = desLayout;
            layouts[i]    = desLayout->GetNativeHandle();
            dynamicNum += desLayout->GetDynamicNum();
        }

        for (const auto &push : des.pushConstants) {
            HashCombine32(hash, Crc32::Cal(push));
        }

        VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
        pipelineLayoutCreateInfo.sType                      = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutCreateInfo.setLayoutCount             = static_cast<uint32_t>(layouts.size());
        pipelineLayoutCreateInfo.pSetLayouts                = layouts.data();
        pipelineLayoutCreateInfo.pushConstantRangeCount     = static_cast<uint32_t>(des.pushConstants.size());
        pipelineLayoutCreateInfo.pPushConstantRanges        = des.pushConstants.data();

        layout = device.GetPipelineLayout(hash, &pipelineLayoutCreateInfo);
        return layout != VK_NULL_HANDLE;
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
