//
// Created by Zach Lee on 2022/1/9.
//

#include <vulkan/DescriptorSetLayout.h>
#include <vulkan/Device.h>
#include <vulkan/Util.h>
#include <vulkan/Conversion.h>

#include <core/hash/Crc32.h>
#include <core/hash/Hash.h>
#include <list>

namespace sky::vk {

    DescriptorSetLayout::DescriptorSetLayout(Device &dev)
        : DevObject(dev), layout(VK_NULL_HANDLE), updateTemplate{VK_NULL_HANDLE}
    {
    }

    DescriptorSetLayout::~DescriptorSetLayout()
    {
        if (updateTemplate != VK_NULL_HANDLE) {
            vkDestroyDescriptorUpdateTemplate(device.GetNativeHandle(), updateTemplate, VKL_ALLOC);
        }
    }

    bool DescriptorSetLayout::Init(const Descriptor &desc)
    {
        descriptorCount = 0;
        bindings = desc.bindings;

        std::vector<VkDescriptorSetLayoutBinding>    bindings;
        std::vector<VkDescriptorBindingFlags>        bindingFlags;
        std::vector<VkDescriptorUpdateTemplateEntry> entries;
        std::list<VkSampler>                         samplers;

        descriptorNum = 0;
        for (const auto &binding : desc.bindings) {
            HashCombine32(hash, Crc32::Cal(binding));

            VkDescriptorSetLayoutBinding layoutBinding = {};
            layoutBinding.binding            = binding.binding;
            layoutBinding.descriptorType     = FromRHI(binding.type);
            layoutBinding.descriptorCount    = binding.count;
            layoutBinding.stageFlags         = FromRHI(binding.visibility);
            layoutBinding.pImmutableSamplers = nullptr;
            bindings.emplace_back(layoutBinding);
            bindingFlags.emplace_back(FromRHI(binding.flags));
            if (IsDynamicDescriptor(layoutBinding.descriptorType)) {
                dynamicNum += binding.count;
            }

            if ((bindingFlags.back() & VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT) == VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT) {
                variableDescriptorCounts.emplace_back(binding.count);
            }

            VkDescriptorUpdateTemplateEntry templateEntry{};
            templateEntry.dstBinding        = binding.binding;
            templateEntry.dstArrayElement   = 0;
            templateEntry.descriptorCount   = binding.count;
            templateEntry.descriptorType    = FromRHI(binding.type);
            templateEntry.offset            = static_cast<uint32_t>(sizeof(DescriptorWriteInfo) * descriptorNum);
            templateEntry.stride            = sizeof(DescriptorWriteInfo);
            bindingIndices[binding.binding] = {descriptorNum, binding.count};
            entries.emplace_back(templateEntry);

            descriptorNum += binding.count;
        }

        VkDescriptorSetLayoutBindingFlagsCreateInfo setLayoutBindingFlags{};
        setLayoutBindingFlags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
        setLayoutBindingFlags.bindingCount = static_cast<uint32_t>(bindingFlags.size());
        setLayoutBindingFlags.pBindingFlags = bindingFlags.data();

        VkDescriptorSetLayoutCreateInfo layoutInfo = {};
        layoutInfo.sType                           = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.pNext                           = &setLayoutBindingFlags;
        layoutInfo.bindingCount                    = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings                       = bindings.data();
        layout                                     = device.GetDescriptorSetLayout(hash, &layoutInfo);
        if (layout == VK_NULL_HANDLE) {
            return false;
        }

        if (!entries.empty()) {
            VkDescriptorUpdateTemplateCreateInfo templateInfo{};
            templateInfo.sType                      = VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO;
            templateInfo.descriptorUpdateEntryCount = static_cast<uint32_t>(entries.size());
            templateInfo.pDescriptorUpdateEntries   = entries.data();
            templateInfo.templateType               = VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_DESCRIPTOR_SET;
            templateInfo.descriptorSetLayout        = layout;
            vkCreateDescriptorUpdateTemplate(device.GetNativeHandle(), &templateInfo, VKL_ALLOC, &updateTemplate);
        }

        return true;
    }

} // namespace sky::vk
