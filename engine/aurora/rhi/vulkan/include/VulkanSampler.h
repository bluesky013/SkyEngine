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

        uint64_t GetId() const { return mId; }

    private:
        VulkanDevice &device;
        VkSampler     sampler = VK_NULL_HANDLE;
        uint64_t      mId     = 0;
    };

} // namespace sky::aurora
