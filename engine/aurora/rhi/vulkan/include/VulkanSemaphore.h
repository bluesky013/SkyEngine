//
// Created on 2026/03/29.
//

#pragma once

#include <aurora/rhi/Semaphore.h>
#include <vulkan/vulkan.h>

namespace sky::aurora {

    class VulkanDevice;

    class VulkanSemaphore : public Semaphore {
    public:
        explicit VulkanSemaphore(VulkanDevice &dev);
        ~VulkanSemaphore() override;

        bool Init(const Descriptor &desc);

        // Backend-specific timeline operations (not exposed in base interface)
        uint64_t GetCurrentValue() const;
        void Wait(uint64_t value);
        void Signal(uint64_t value);

        VkSemaphore GetNativeHandle() const { return semaphore; }

    private:
        VulkanDevice &device;
        VkSemaphore   semaphore = VK_NULL_HANDLE;
    };

} // namespace sky::aurora
