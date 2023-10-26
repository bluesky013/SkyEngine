//
// Created by Zach Lee on 2022/1/9.
//

#include <core/hash/Crc32.h>
#include <core/logger/Logger.h>
#include <vulkan/Basic.h>
#include <vulkan/Device.h>
#include <vulkan/Shader.h>
#include <vulkan/Conversion.h>
static const char *TAG = "Vulkan";

namespace sky::vk {

    Shader::Shader(Device &dev) : DevObject(dev), shaderModule(VK_NULL_HANDLE), stage(VK_SHADER_STAGE_VERTEX_BIT), hash(0)
    {
    }

    Shader::~Shader()
    {
        if (shaderModule != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device.GetNativeHandle(), shaderModule, VKL_ALLOC);
        }
    }

    bool Shader::Init(const Descriptor &des)
    {
        VkShaderModuleCreateInfo shaderInfo = {};
        shaderInfo.sType                    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        shaderInfo.codeSize                 = des.size;
        shaderInfo.pCode                    = reinterpret_cast<const uint32_t *>(des.data);
        stage                               = static_cast<VkShaderStageFlagBits>(FromRHI(des.stage));

        auto rst = vkCreateShaderModule(device.GetNativeHandle(), &shaderInfo, VKL_ALLOC, &shaderModule);
        if (rst != VK_SUCCESS) {
            LOG_E(TAG, "create shader module failed %d", rst);
            return false;
        }
        hash = Crc32::Cal(reinterpret_cast<const uint8_t *>(shaderInfo.pCode), static_cast<uint32_t>(shaderInfo.codeSize));
        return true;
    }

    bool Shader::Init(const VkDescriptor &des)
    {
        VkShaderModuleCreateInfo shaderInfo = {};
        shaderInfo.sType                    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        shaderInfo.codeSize                 = des.size;
        shaderInfo.pCode                    = des.spv;
        stage                               = des.stage;

        auto rst = vkCreateShaderModule(device.GetNativeHandle(), &shaderInfo, VKL_ALLOC, &shaderModule);
        if (rst != VK_SUCCESS) {
            LOG_E(TAG, "create shader module failed %d", rst);
            return false;
        }
        hash = Crc32::Cal(reinterpret_cast<const uint8_t *>(shaderInfo.pCode), static_cast<uint32_t>(shaderInfo.codeSize));
        return true;
    }

    VkShaderModule Shader::GetNativeHandle() const
    {
        return shaderModule;
    }

    VkShaderStageFlagBits Shader::GetShaderStage() const
    {
        return stage;
    }

    uint32_t Shader::GetHash() const
    {
        return hash;
    }
} // namespace sky::vk
