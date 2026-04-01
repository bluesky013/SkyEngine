//
// Created on 2026/04/02.
//

#pragma once

#include <aurora/rhi/Sampler.h>
#include <vulkan/vulkan.h>

namespace sky::aurora {

    class VulkanDevice;

    class VulkanSampler : public Sampler {
    public:
        explicit VulkanSampler(VulkanDevice &dev);
        ~VulkanSampler() override;

        bool Init(const Descriptor &desc);

        VkSampler GetNativeHandle() const { return sampler; }

    private:
        VulkanDevice &device;
        VkSampler     sampler = VK_NULL_HANDLE;
    };

} // namespace sky::aurora
