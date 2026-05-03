//
// Created on 2026/03/29.
//

#include <VulkanSemaphore.h>
#include <VulkanDevice.h>
#include <core/logger/Logger.h>
#include <core/platform/Platform.h>

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
        type = desc.type;

        VkSemaphoreTypeCreateInfo typeInfo = {};
        typeInfo.sType         = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
        typeInfo.semaphoreType = (type == SemaphoreType::TIMELINE)
                                     ? VK_SEMAPHORE_TYPE_TIMELINE
                                     : VK_SEMAPHORE_TYPE_BINARY;
        typeInfo.initialValue  = (type == SemaphoreType::TIMELINE) ? desc.initialValue : 0;

        VkSemaphoreCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        createInfo.pNext = &typeInfo;

        VkResult result = device.GetDeviceFn().vkCreateSemaphore(device.GetNativeHandle(), &createInfo, nullptr, &semaphore);
        if (result != VK_SUCCESS) {
            LOG_E(TAG, "failed to create VkSemaphore (type=%u), error: %d", static_cast<uint32_t>(type), result);
            return false;
        }
        return true;
    }

    uint64_t VulkanSemaphore::GetCurrentValue() const
    {
        SKY_ASSERT(type == SemaphoreType::TIMELINE);
        uint64_t value = 0;
        device.GetDeviceFn().vkGetSemaphoreCounterValue(device.GetNativeHandle(), semaphore, &value);
        return value;
    }

    bool VulkanSemaphore::Wait(uint64_t value, uint64_t timeoutNs)
    {
        SKY_ASSERT(type == SemaphoreType::TIMELINE);
        VkSemaphoreWaitInfo waitInfo = {};
        waitInfo.sType          = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
        waitInfo.semaphoreCount = 1;
        waitInfo.pSemaphores    = &semaphore;
        waitInfo.pValues        = &value;

        const VkResult res = device.GetDeviceFn().vkWaitSemaphores(device.GetNativeHandle(), &waitInfo, timeoutNs);
        return res == VK_SUCCESS;
    }

    void VulkanSemaphore::Signal(uint64_t value)
    {
        SKY_ASSERT(type == SemaphoreType::TIMELINE);
        VkSemaphoreSignalInfo signalInfo = {};
        signalInfo.sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO;
        signalInfo.semaphore = semaphore;
        signalInfo.value     = value;

        device.GetDeviceFn().vkSignalSemaphore(device.GetNativeHandle(), &signalInfo);
    }

} // namespace sky::aurora
