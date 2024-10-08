//
// Created by Zach Lee on 2022/1/9.
//

#pragma once

#include "rhi/Sampler.h"
#include "vulkan/DevObject.h"
#include "vulkan/vulkan.h"

namespace sky::vk {

    class Device;

    class Sampler : public rhi::Sampler, public DevObject {
    public:
        ~Sampler();

        struct VkDescriptor {
            VkFilter             magFilter        = VK_FILTER_NEAREST;
            VkFilter             minFilter        = VK_FILTER_NEAREST;
            VkSamplerMipmapMode  mipmapMode       = VK_SAMPLER_MIPMAP_MODE_NEAREST;
            VkSamplerAddressMode addressModeU     = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            VkSamplerAddressMode addressModeV     = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            VkSamplerAddressMode addressModeW     = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            VkBool32             anisotropyEnable = VK_FALSE;
            float                maxAnisotropy    = 0.f;
            float                minLod           = 0.f;
            float                maxLod           = 0.25f;
            VkBorderColor        borderColor      = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
        };

        VkSampler GetNativeHandle() const;

    private:
        friend class Device;
        Sampler(Device &);

        bool Init(const Descriptor &);
        bool Init(const VkDescriptor &);

        VkSampler sampler;
        uint32_t  hash = 0;
    };

    using SamplerPtr = std::shared_ptr<Sampler>;

} // namespace sky::vk
