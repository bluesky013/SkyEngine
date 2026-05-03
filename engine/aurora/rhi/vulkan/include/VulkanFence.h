//
// Created on 2026/03/29.
//

#pragma once

#include <aurora/rhi/Fence.h>
#include <vulkan/vulkan.h>

namespace sky::aurora {

    class VulkanDevice;

    class VulkanFence : public Fence {
    public:
        explicit VulkanFence(VulkanDevice &dev);
        ~VulkanFence() override;

        bool Init(const Descriptor &desc);

        void Wait() override;
        void Reset() override;
        bool IsSignaled() override;
        bool WaitFor(uint64_t timeoutNs) override;

        VkFence GetNativeHandle() const { return fence; }

    private:
        VulkanDevice &device;
        VkFence       fence = VK_NULL_HANDLE;
    };

} // namespace sky::aurora
