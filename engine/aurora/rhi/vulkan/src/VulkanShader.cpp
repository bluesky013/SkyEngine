//
// Created by Zach Lee on 2026/4/7.
//

#include <VulkanShader.h>

#include <VulkanConversion.h>
#include <VulkanDevice.h>
#include <core/logger/Logger.h>

#include <cstring>
#include <map>

static const char *TAG = "VulkanShader";

namespace sky::aurora {

    const Device *VulkanShaderFunction::GetDevice() const noexcept
    {
        return &device;
    }

    const Device *VulkanShader::GetDevice() const noexcept
    {
        return &device;
    }

    VulkanShaderFunction::VulkanShaderFunction(VulkanDevice &dev) : device(dev)
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
        std::string message;
        if (!InitArtifact(desc, API::VULKAN, message)) {
            LOG_E(TAG, "%s", message.c_str());
            return false;
        }
        const auto              &bytes = artifact->data;
        VkShaderModuleCreateInfo ci    = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        ci.codeSize                    = bytes.size();
        std::vector<uint32_t> words(bytes.size() / sizeof(uint32_t));
        std::memcpy(words.data(), bytes.data(), bytes.size());
        ci.pCode = words.data();

        const VkResult result = device.GetDeviceFn().vkCreateShaderModule(device.GetNativeHandle(), &ci, nullptr, &shaderModule);
        if (result != VK_SUCCESS) {
            LOG_E(TAG, "vkCreateShaderModule failed, VkResult=%d", static_cast<int>(result));
            return false;
        }

        stage = desc.stage;
        return true;
    }

    VulkanShader::VulkanShader(VulkanDevice &dev) : device(dev)
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

        std::string error;
        if (!BuildVulkanBindingPlan(reflection, device.GetLimits().maxBoundDescriptorSets, bindingPlan, error)) {
            LOG_E(TAG, "%s", error.c_str());
            return false;
        }
        std::map<uint32_t, std::vector<VkDescriptorSetLayoutBinding>>    setBindings;
        std::map<uint32_t, std::vector<VkDescriptorUpdateTemplateEntry>> setTemplateEntries;
        for (const auto &[set, plan] : bindingPlan.sets) {
            for (const auto &resource : plan.bindings) {
                VkDescriptorSetLayoutBinding binding{};
                binding.binding         = resource.physicalBinding;
                binding.descriptorType  = FromShaderResourceType(resource.type);
                binding.descriptorCount = resource.count;
                binding.stageFlags      = FromShaderStageFlags(resource.stages);
                if (binding.descriptorType == VK_DESCRIPTOR_TYPE_MAX_ENUM || !binding.stageFlags) {
                    return false;
                }
                setBindings[set].push_back(binding);
                VkDescriptorUpdateTemplateEntry entry{};
                entry.dstBinding      = resource.physicalBinding;
                entry.descriptorCount = resource.count;
                entry.descriptorType  = binding.descriptorType;
                entry.offset          = sizeof(DescriptorWriteInfo) * resource.slotBase;
                entry.stride          = sizeof(DescriptorWriteInfo);
                setTemplateEntries[set].push_back(entry);
            }
        }

        // Pipeline layout positions are real set indices, including unused holes.
        descriptorSetLayouts.clear();
        std::vector<VkDescriptorSetLayout> setLayouts;
        const uint32_t                     setCount = setBindings.empty() ? 0 : setBindings.rbegin()->first + 1;
        if (!setBindings.empty() && setBindings.rbegin()->first >= device.GetLimits().maxBoundDescriptorSets)
            return false;
        setLayouts.reserve(setCount);
        for (uint32_t set = 0; set < setCount; ++set) {
            const auto                     &bindings = setBindings[set];
            VkDescriptorSetLayoutCreateInfo ci       = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
            ci.bindingCount                          = static_cast<uint32_t>(bindings.size());
            ci.pBindings                             = bindings.data();

            VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
            const VkResult        r         = device.GetDeviceFn().vkCreateDescriptorSetLayout(device.GetNativeHandle(), &ci, nullptr, &setLayout);
            if (r != VK_SUCCESS) {
                LOG_E(TAG, "vkCreateDescriptorSetLayout failed, VkResult=%d", static_cast<int>(r));
                return false;
            }
            descriptorSetLayouts[set] = setLayout;
            setLayouts.push_back(setLayout);
        }

        // create one VkDescriptorUpdateTemplate per set (Vulkan 1.1 core, always
        // available on our 1.3 floor). Failure leaves VK_NULL_HANDLE -> encoder
        // falls back to vkUpdateDescriptorSets.
        descriptorUpdateTemplates.clear();
        for (auto &entry : setTemplateEntries) {
            const uint32_t set      = entry.first;
            auto           layoutIt = descriptorSetLayouts.find(set);
            if (layoutIt == descriptorSetLayouts.end() || entry.second.empty()) {
                continue;
            }
            VkDescriptorUpdateTemplateCreateInfo tplInfo = {VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO};
            tplInfo.descriptorUpdateEntryCount           = static_cast<uint32_t>(entry.second.size());
            tplInfo.pDescriptorUpdateEntries             = entry.second.data();
            tplInfo.templateType                         = VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_DESCRIPTOR_SET;
            tplInfo.descriptorSetLayout                  = layoutIt->second;

            VkDescriptorUpdateTemplate tpl = VK_NULL_HANDLE;
            const VkResult             tr  = device.GetDeviceFn().vkCreateDescriptorUpdateTemplate(device.GetNativeHandle(), &tplInfo, nullptr, &tpl);
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
            range.stageFlags          = FromShaderStageFlags(pc.stageFlags);
            range.offset              = pc.offset;
            range.size                = pc.size;
            pushRanges.push_back(range);
        }

        VkPipelineLayoutCreateInfo createInfo = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        createInfo.setLayoutCount             = static_cast<uint32_t>(setLayouts.size());
        createInfo.pSetLayouts                = setLayouts.empty() ? nullptr : setLayouts.data();
        createInfo.pushConstantRangeCount     = static_cast<uint32_t>(pushRanges.size());
        createInfo.pPushConstantRanges        = pushRanges.empty() ? nullptr : pushRanges.data();

        const VkResult result = device.GetDeviceFn().vkCreatePipelineLayout(device.GetNativeHandle(), &createInfo, nullptr, &layout);
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
        std::string message;
        if (!BuildShaderProgramReflection(desc, device, reflection, message)) {
            LOG_E(TAG, "%s", message.c_str());
            return false;
        }
        if (!InitParameterTable(desc, message)) {
            LOG_E(TAG, "%s", message.c_str());
            return false;
        }
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
