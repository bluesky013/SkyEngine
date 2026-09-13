//
// Created by Zach Lee on 2026/4/7.
//

#include <VulkanShader.h>
#include <VulkanDevice.h>
#include <VulkanConversion.h>
#include <core/logger/Logger.h>

#include <map>

static const char *TAG = "VulkanShader";

namespace sky::aurora {

    // -----------------------------------------------------------------------
    // VulkanShaderFunction
    // -----------------------------------------------------------------------
    VulkanShaderFunction::VulkanShaderFunction(VulkanDevice &dev)
        : device(dev)
    {
    }

    VulkanShaderFunction::~VulkanShaderFunction()
    {
        if (shaderModule != VK_NULL_HANDLE) {
            device.GetDeviceFn().vkDestroyShaderModule(device.GetNativeHandle(), shaderModule, nullptr);
        }
    }

    bool VulkanShaderFunction::Init(const Descriptor &desc)
    {
        if (desc.data == nullptr) {
            LOG_E(TAG, "shader function requires shader data");
            return false;
        }

        const auto *binaryProvider = static_cast<const ShaderBinaryProvider *>(desc.data.Get());
        if (binaryProvider->binaryData == nullptr) {
            LOG_E(TAG, "shader function missing binary payload");
            return false;
        }

        const auto &binary = binaryProvider->binaryData;
        if (binary->Size() == 0 || binary->Size() % 4 != 0) {
            LOG_E(TAG, "invalid SPIR-V binary size: %zu", binary->Size());
            return false;
        }

        VkShaderModuleCreateInfo ci = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        ci.codeSize = binary->Size();
        ci.pCode    = reinterpret_cast<const uint32_t *>(binary->Data());

        const VkResult result = device.GetDeviceFn().vkCreateShaderModule(device.GetNativeHandle(), &ci, nullptr, &shaderModule);
        if (result != VK_SUCCESS) {
            LOG_E(TAG, "vkCreateShaderModule failed, VkResult=%d", static_cast<int>(result));
            return false;
        }

        stage = desc.stage;
        return true;
    }

    // -----------------------------------------------------------------------
    // VulkanShader
    // -----------------------------------------------------------------------
    VulkanShader::VulkanShader(VulkanDevice &dev)
        : device(dev)
    {
    }

    VulkanShader::~VulkanShader()
    {
        if (layout != VK_NULL_HANDLE) {
            device.GetDeviceFn().vkDestroyPipelineLayout(device.GetNativeHandle(), layout, nullptr);
        }
        for (auto &kv : descriptorSetLayouts) {
            device.GetDeviceFn().vkDestroyDescriptorSetLayout(device.GetNativeHandle(), kv.second, nullptr);
        }
        for (auto &kv : descriptorUpdateTemplates) {
            device.GetDeviceFn().vkDestroyDescriptorUpdateTemplate(device.GetNativeHandle(), kv.second, nullptr);
        }
    }

    bool VulkanShader::CreatePipelineLayout()
    {
        if (layout != VK_NULL_HANDLE) {
            return true;
        }

        // group reflected bindings by set index; also accumulate update template
        // entries (slot offset into the packed DescriptorWriteInfo buffer)
        std::map<uint32_t, std::vector<VkDescriptorSetLayoutBinding>> setBindings;
        std::map<uint32_t, std::vector<VkDescriptorUpdateTemplateEntry>> setTemplateEntries;
        std::map<uint32_t, uint32_t> setSlotBase;
        for (const auto &res : reflection.resources) {
            VkDescriptorSetLayoutBinding binding = {};
            binding.binding         = res.binding;
            binding.descriptorType  = FromShaderResourceType(res.type);
            binding.descriptorCount = res.count;
            binding.stageFlags      = VK_SHADER_STAGE_ALL;
            binding.pImmutableSamplers = nullptr;
            setBindings[res.set].push_back(binding);

            VkDescriptorUpdateTemplateEntry entry = {};
            entry.dstBinding      = res.binding;
            entry.dstArrayElement = 0;
            entry.descriptorCount = res.count;
            entry.descriptorType  = FromShaderResourceType(res.type);
            entry.offset          = static_cast<uint32_t>(sizeof(DescriptorWriteInfo) * setSlotBase[res.set]);
            entry.stride          = static_cast<uint32_t>(sizeof(DescriptorWriteInfo));
            setTemplateEntries[res.set].push_back(entry);
            setSlotBase[res.set] += res.count;
        }

        // create one VkDescriptorSetLayout per set (keyed by real set index)
        descriptorSetLayouts.clear();
        std::vector<VkDescriptorSetLayout> setLayouts;
        setLayouts.reserve(setBindings.size());
        for (auto &entry : setBindings) {
            VkDescriptorSetLayoutCreateInfo ci = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
            ci.bindingCount = static_cast<uint32_t>(entry.second.size());
            ci.pBindings    = entry.second.data();

            VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
            const VkResult r = device.GetDeviceFn().vkCreateDescriptorSetLayout(
                device.GetNativeHandle(), &ci, nullptr, &setLayout);
            if (r != VK_SUCCESS) {
                LOG_E(TAG, "vkCreateDescriptorSetLayout failed, VkResult=%d", static_cast<int>(r));
                return false;
            }
            descriptorSetLayouts[entry.first] = setLayout;
            setLayouts.push_back(setLayout);
        }

        // create one VkDescriptorUpdateTemplate per set (Vulkan 1.1 core, always
        // available on our 1.3 floor). Failure leaves VK_NULL_HANDLE -> encoder
        // falls back to vkUpdateDescriptorSets.
        descriptorUpdateTemplates.clear();
        for (auto &entry : setTemplateEntries) {
            const uint32_t set = entry.first;
            auto layoutIt = descriptorSetLayouts.find(set);
            if (layoutIt == descriptorSetLayouts.end() || entry.second.empty()) {
                continue;
            }
            VkDescriptorUpdateTemplateCreateInfo tplInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO};
            tplInfo.descriptorUpdateEntryCount = static_cast<uint32_t>(entry.second.size());
            tplInfo.pDescriptorUpdateEntries   = entry.second.data();
            tplInfo.templateType               = VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_DESCRIPTOR_SET;
            tplInfo.descriptorSetLayout        = layoutIt->second;

            VkDescriptorUpdateTemplate tpl = VK_NULL_HANDLE;
            const VkResult tr = device.GetDeviceFn().vkCreateDescriptorUpdateTemplate(
                device.GetNativeHandle(), &tplInfo, nullptr, &tpl);
            if (tr == VK_SUCCESS) {
                descriptorUpdateTemplates[set] = tpl;
            } else {
                LOG_E(TAG, "vkCreateDescriptorUpdateTemplate failed for set %u: %d", set, static_cast<int>(tr));
            }
        }

        // push constants
        std::vector<VkPushConstantRange> pushRanges;
        pushRanges.reserve(reflection.pushConstants.size());
        for (const auto &pc : reflection.pushConstants) {
            VkPushConstantRange range = {};
            range.stageFlags = FromShaderStageFlags(pc.stageFlags);
            range.offset     = pc.offset;
            range.size       = pc.size;
            pushRanges.push_back(range);
        }

        VkPipelineLayoutCreateInfo createInfo = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        createInfo.setLayoutCount         = static_cast<uint32_t>(setLayouts.size());
        createInfo.pSetLayouts            = setLayouts.empty() ? nullptr : setLayouts.data();
        createInfo.pushConstantRangeCount = static_cast<uint32_t>(pushRanges.size());
        createInfo.pPushConstantRanges    = pushRanges.empty() ? nullptr : pushRanges.data();

        const VkResult result = device.GetDeviceFn().vkCreatePipelineLayout(
            device.GetNativeHandle(), &createInfo, nullptr, &layout);
        if (result != VK_SUCCESS) {
            LOG_E(TAG, "vkCreatePipelineLayout failed, VkResult=%d", static_cast<int>(result));
            return false;
        }
        return true;
    }

    VkDescriptorSetLayout VulkanShader::GetDescriptorSetLayout(uint32_t set) const
    {
        auto it = descriptorSetLayouts.find(set);
        return it != descriptorSetLayouts.end() ? it->second : VK_NULL_HANDLE;
    }

    VkDescriptorUpdateTemplate VulkanShader::GetDescriptorUpdateTemplate(uint32_t set) const
    {
        auto it = descriptorUpdateTemplates.find(set);
        return it != descriptorUpdateTemplates.end() ? it->second : VK_NULL_HANDLE;
    }

    bool VulkanShader::Init(const Descriptor &desc)
    {
        if (desc.reflection == nullptr) {
            LOG_E(TAG, "shader requires a non-null reflection");
            return false;
        }
        reflection = *desc.reflection;
        if (desc.specialization != nullptr) {
            specialization = *desc.specialization;
        }

        // Shader::Descriptor is a union where cs and vs share the same memory.
        // Check ps to distinguish graphics (vs+ps) from compute (cs only).
        if (desc.ps != nullptr) {
            vertexFunction   = static_cast<VulkanShaderFunction *>(desc.vs);
            fragmentFunction = static_cast<VulkanShaderFunction *>(desc.ps);
            return vertexFunction != nullptr && CreatePipelineLayout();
        }

        if (desc.cs != nullptr) {
            computeFunction = static_cast<VulkanShaderFunction *>(desc.cs);
            return computeFunction != nullptr && CreatePipelineLayout();
        }

        return false;
    }

} // namespace sky::aurora
