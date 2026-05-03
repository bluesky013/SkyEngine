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

        SemaphoreType GetType() const override { return type; }
        void          Signal(uint64_t value) override;
        bool          Wait(uint64_t value, uint64_t timeoutNs) override;
        uint64_t      GetCurrentValue() const override;

        VkSemaphore GetNativeHandle() const { return semaphore; }

    private:
        VulkanDevice &device;
        VkSemaphore   semaphore = VK_NULL_HANDLE;
        SemaphoreType type      = SemaphoreType::BINARY;
    };

} // namespace sky::aurora
