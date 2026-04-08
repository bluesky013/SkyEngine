//
// Created on 2026/04/02.
//

#include "VulkanSampler.h"
#include "VulkanDevice.h"
#include "VulkanConversion.h"
#include <core/logger/Logger.h>

static const char *TAG = "AuroraVulkan";

namespace sky::aurora {

    VulkanSampler::VulkanSampler(VulkanDevice &dev)
        : device(dev)
    {
    }

    VulkanSampler::~VulkanSampler()
    {
        if (sampler != VK_NULL_HANDLE) {
            device.GetDeviceFn().vkDestroySampler(device.GetNativeHandle(), sampler, nullptr);
        }
    }

    bool VulkanSampler::Init(const Descriptor &desc)
    {
        const bool anisotropyEnabled = desc.anisotropyEnable && device.GetCapability().anisotropyEnable;

        VkSamplerCreateInfo samplerCI = {};
        samplerCI.sType            = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerCI.magFilter        = FromFilter(desc.magFilter);
        samplerCI.minFilter        = FromFilter(desc.minFilter);
        samplerCI.mipmapMode       = FromMipFilter(desc.mipmapMode);
        samplerCI.addressModeU     = FromWrapMode(desc.addressModeU);
        samplerCI.addressModeV     = FromWrapMode(desc.addressModeV);
        samplerCI.addressModeW     = FromWrapMode(desc.addressModeW);
        samplerCI.mipLodBias       = 0.f;
        samplerCI.anisotropyEnable = anisotropyEnabled ? VK_TRUE : VK_FALSE;
        samplerCI.maxAnisotropy    = anisotropyEnabled ? desc.maxAnisotropy : 1.f;
        samplerCI.compareEnable    = VK_FALSE;
        samplerCI.compareOp        = VK_COMPARE_OP_NEVER;
        samplerCI.minLod           = desc.minLod;
        samplerCI.maxLod           = desc.maxLod;
        samplerCI.borderColor      = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
        samplerCI.unnormalizedCoordinates = VK_FALSE;

        if (desc.anisotropyEnable && !anisotropyEnabled) {
            LOG_W(TAG, "sampler anisotropy requested but not supported by the current Vulkan device; falling back to anisotropy disabled");
        }

        const VkResult res = device.GetDeviceFn().vkCreateSampler(device.GetNativeHandle(), &samplerCI, nullptr, &sampler);
        if (res != VK_SUCCESS) {
            LOG_E(TAG, "vkCreateSampler failed: %d", res);
            return false;
        }

        return true;
    }

} // namespace sky::aurora
