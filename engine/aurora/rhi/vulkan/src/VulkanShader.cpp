//
// Created by Zach Lee on 2026/4/7.
//

#include <VulkanShader.h>
#include <VulkanDevice.h>
#include <VulkanConversion.h>
#include <core/logger/Logger.h>

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

    bool VulkanShader::Init(const Descriptor &desc)
    {
        // Shader::Descriptor is a union where cs and vs share the same memory.
        // Check ps to distinguish graphics (vs+ps) from compute (cs only).
        if (desc.ps != nullptr) {
            vertexFunction   = static_cast<VulkanShaderFunction *>(desc.vs);
            fragmentFunction = static_cast<VulkanShaderFunction *>(desc.ps);
            return vertexFunction != nullptr;
        }

        if (desc.cs != nullptr) {
            computeFunction = static_cast<VulkanShaderFunction *>(desc.cs);
            return computeFunction != nullptr;
        }

        return false;
    }

} // namespace sky::aurora
