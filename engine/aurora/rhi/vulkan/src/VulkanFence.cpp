//
// Created on 2026/03/29.
//

#include <VulkanFence.h>
#include <VulkanDevice.h>
#include <core/logger/Logger.h>

static const char *TAG = "AuroraVulkan";

namespace sky::aurora {

    VulkanFence::VulkanFence(VulkanDevice &dev)
        : device(dev)
    {
    }

    VulkanFence::~VulkanFence()
    {
        if (fence != VK_NULL_HANDLE) {
            vkDestroyFence(device.GetNativeHandle(), fence, nullptr);
        }
    }

    bool VulkanFence::Init(const Descriptor &desc)
    {
        VkFenceCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        if (desc.createSignaled) {
            createInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        }

        VkResult result = vkCreateFence(device.GetNativeHandle(), &createInfo, nullptr, &fence);
        if (result != VK_SUCCESS) {
            LOG_E(TAG, "failed to create VkFence, error: %d", result);
            return false;
        }
        return true;
    }

    void VulkanFence::Wait()
    {
        vkWaitForFences(device.GetNativeHandle(), 1, &fence, VK_TRUE, UINT64_MAX);
    }

    void VulkanFence::Reset()
    {
        vkResetFences(device.GetNativeHandle(), 1, &fence);
    }

} // namespace sky::aurora
