//
// Created on 2026/03/29.
//

#include <VulkanSemaphore.h>
#include <VulkanDevice.h>
#include <core/logger/Logger.h>

static const char *TAG = "AuroraVulkan";

namespace sky::aurora {

    VulkanSemaphore::VulkanSemaphore(VulkanDevice &dev)
        : device(dev)
    {
    }

    VulkanSemaphore::~VulkanSemaphore()
    {
        if (semaphore != VK_NULL_HANDLE) {
            device.GetDeviceFn().vkDestroySemaphore(device.GetNativeHandle(), semaphore, nullptr);
        }
    }

    bool VulkanSemaphore::Init(const Descriptor &desc)
    {
        VkSemaphoreTypeCreateInfo typeInfo = {};
        typeInfo.sType         = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
        typeInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
        typeInfo.initialValue  = desc.initialValue;

        VkSemaphoreCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        createInfo.pNext = &typeInfo;

        VkResult result = device.GetDeviceFn().vkCreateSemaphore(device.GetNativeHandle(), &createInfo, nullptr, &semaphore);
        if (result != VK_SUCCESS) {
            LOG_E(TAG, "failed to create timeline VkSemaphore, error: %d", result);
            return false;
        }
        return true;
    }

    uint64_t VulkanSemaphore::GetCurrentValue() const
    {
        uint64_t value = 0;
        device.GetDeviceFn().vkGetSemaphoreCounterValue(device.GetNativeHandle(), semaphore, &value);
        return value;
    }

    void VulkanSemaphore::Wait(uint64_t value)
    {
        VkSemaphoreWaitInfo waitInfo = {};
        waitInfo.sType          = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
        waitInfo.semaphoreCount = 1;
        waitInfo.pSemaphores    = &semaphore;
        waitInfo.pValues        = &value;

        device.GetDeviceFn().vkWaitSemaphores(device.GetNativeHandle(), &waitInfo, UINT64_MAX);
    }

    void VulkanSemaphore::Signal(uint64_t value)
    {
        VkSemaphoreSignalInfo signalInfo = {};
        signalInfo.sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO;
        signalInfo.semaphore = semaphore;
        signalInfo.value     = value;

        device.GetDeviceFn().vkSignalSemaphore(device.GetNativeHandle(), &signalInfo);
    }

} // namespace sky::aurora
