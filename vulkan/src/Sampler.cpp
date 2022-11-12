//
// Created by Zach Lee on 2022/1/9.
//

#include <core/hash/Crc32.h>
#include <core/logger/Logger.h>
#include <vulkan/Basic.h>
#include <vulkan/Device.h>
#include <vulkan/Sampler.h>
static const char *TAG = "Vulkan";

namespace sky::vk {

    Sampler::Sampler(Device &dev) : DevObject(dev), sampler(VK_NULL_HANDLE)
    {
    }

    Sampler::~Sampler()
    {
    }

    bool Sampler::Init(const VkDescriptor &des)
    {
        hash = Crc32::Cal(reinterpret_cast<const uint8_t *>(&des), sizeof(VkDescriptor));

        VkSamplerCreateInfo samplerInfo = {};
        samplerInfo.sType               = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter           = des.magFilter;
        samplerInfo.minFilter           = des.minFilter;
        samplerInfo.mipmapMode          = des.mipmapMode;
        samplerInfo.addressModeU        = des.addressModeU;
        samplerInfo.addressModeV        = des.addressModeV;
        samplerInfo.addressModeW        = des.addressModeW;
        samplerInfo.anisotropyEnable    = des.anisotropyEnable;
        samplerInfo.maxAnisotropy       = des.maxAnisotropy;
        samplerInfo.compareEnable       = VK_FALSE;
        samplerInfo.compareOp           = VK_COMPARE_OP_NEVER;
        samplerInfo.minLod              = des.minLod;
        samplerInfo.maxLod              = des.maxLod;
        samplerInfo.borderColor         = des.borderColor;

        sampler = device.GetSampler(hash, &samplerInfo);
        if (sampler == VK_NULL_HANDLE) {
            return false;
        }

        return true;
    }

    VkSampler Sampler::GetNativeHandle() const
    {
        return sampler;
    }

} // namespace sky::vk
