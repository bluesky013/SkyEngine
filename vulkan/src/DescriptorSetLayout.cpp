//
// Created by Zach Lee on 2022/1/9.
//

#include <vulkan/DescriptorSetLayout.h>
#include <vulkan/Device.h>
#include <vulkan/Util.h>

#include <core/hash/Crc32.h>
#include <core/hash/Hash.h>
#include <list>

namespace sky::drv {

    DescriptorSetLayout::DescriptorSetLayout(Device &dev)
    : DevObject(dev), layout(VK_NULL_HANDLE), updateTemplate{VK_NULL_HANDLE}, dynamicNum(0), descriptorNum(0), hash(0)
    {
    }

    DescriptorSetLayout::~DescriptorSetLayout()
    {
        if (updateTemplate.handle != VK_NULL_HANDLE) {
            vkDestroyDescriptorUpdateTemplate(device.GetNativeHandle(), updateTemplate.handle, VKL_ALLOC);
        }
    }

    bool DescriptorSetLayout::Init(const Descriptor &des)
    {
        info = des;

        std::vector<VkDescriptorSetLayoutBinding>    bindings;
        std::vector<VkDescriptorBindingFlags>        bindingFlags;
        std::vector<VkDescriptorUpdateTemplateEntry> entries;
        std::list<VkSampler>                         samplers;

        descriptorNum = 0;
        for (auto &[binding, desInfo] : des.bindings) {
            HashCombine32(hash, Crc32::Cal(binding));
            HashCombine32(hash, Crc32::Cal(desInfo));

            VkDescriptorSetLayoutBinding layoutBinding = {};
            layoutBinding.binding                      = binding;
            layoutBinding.descriptorType               = desInfo.descriptorType;
            layoutBinding.descriptorCount              = desInfo.descriptorCount;
            layoutBinding.stageFlags                   = desInfo.stageFlags;
            layoutBinding.pImmutableSamplers           = nullptr;
            bindings.emplace_back(layoutBinding);
            bindingFlags.emplace_back(desInfo.bindingFlags);

            if (IsDynamicDescriptor(layoutBinding.descriptorType)) {
                ++dynamicNum;
            }

            VkDescriptorUpdateTemplateEntry templateEntry{};
            templateEntry.dstBinding        = binding;
            templateEntry.dstArrayElement   = 0;
            templateEntry.descriptorCount   = desInfo.descriptorCount;
            templateEntry.descriptorType    = desInfo.descriptorType;
            templateEntry.offset            = static_cast<uint32_t>(sizeof(DescriptorWriteInfo) * descriptorNum);
            templateEntry.stride            = sizeof(DescriptorWriteInfo);
            updateTemplate.indices[binding] = descriptorNum;
            entries.emplace_back(templateEntry);

            descriptorNum += desInfo.descriptorCount;
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

        VkDescriptorUpdateTemplateCreateInfo templateInfo{};
        templateInfo.sType                      = VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO;
        templateInfo.descriptorUpdateEntryCount = static_cast<uint32_t>(entries.size());
        templateInfo.pDescriptorUpdateEntries   = entries.data();
        templateInfo.templateType               = VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_DESCRIPTOR_SET;
        templateInfo.descriptorSetLayout        = layout;
        vkCreateDescriptorUpdateTemplate(device.GetNativeHandle(), &templateInfo, VKL_ALLOC, &updateTemplate.handle);

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

    uint32_t DescriptorSetLayout::GetDynamicNum() const
    {
        return dynamicNum;
    }

    uint32_t DescriptorSetLayout::GetDescriptorNum() const
    {
        return descriptorNum;
    }

    const std::map<uint32_t, DescriptorSetLayout::SetBinding> &DescriptorSetLayout::GetDescriptorTable() const
    {
        return info.bindings;
    }

    const UpdateTemplate &DescriptorSetLayout::GetUpdateTemplate() const
    {
        return updateTemplate;
    }

} // namespace sky::drv
