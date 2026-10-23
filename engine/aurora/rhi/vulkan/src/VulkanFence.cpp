//
// Created on 2026/03/29.
//

#include <VulkanFence.h>

#include <VulkanDevice.h>
#include <core/logger/Logger.h>

static const char *TAG = "AuroraVulkan";

namespace sky::aurora {

    const Device *VulkanFence::GetDevice() const noexcept
    {
        return &device;
    }

    VulkanFence::VulkanFence(VulkanDevice &dev) : device(dev)
    {
    }

    VulkanFence::~VulkanFence()
    {
        if (fence != VK_NULL_HANDLE) {
            device.GetDeviceFn().vkDestroyFence(device.GetNativeHandle(), fence, nullptr);
        }
    }

    bool VulkanFence::Init(const Descriptor &desc)
    {
        VkFenceCreateInfo createInfo = {};
        createInfo.sType             = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        if (desc.createSignaled) {
            createInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        }

        VkResult result = device.GetDeviceFn().vkCreateFence(device.GetNativeHandle(), &createInfo, nullptr, &fence);
        if (result != VK_SUCCESS) {
            LOG_E(TAG, "failed to create VkFence, error: %d", result);
            return false;
        }
        return true;
    }

    void VulkanFence::Wait()
    {
        device.GetDeviceFn().vkWaitForFences(device.GetNativeHandle(), 1, &fence, VK_TRUE, UINT64_MAX);
    }

    void VulkanFence::Reset()
    {
        const auto result = TryReset();
        if (!result.Succeeded())
            LOG_E(TAG, "Fence reset failed: %lld", static_cast<long long>(result.error.nativeCode));
    }

    bool VulkanFence::IsSignaled()
    {
        return Query().status == FenceStatus::SIGNALED;
    }

    GpuOperationResult VulkanFence::TryReset() noexcept
    {
        const auto result = device.GetDeviceFn().vkResetFences(device.GetNativeHandle(), 1, &fence);
        if (result == VK_SUCCESS)
            return {};
        return {{result == VK_ERROR_DEVICE_LOST ? GpuErrorCode::DEVICE_LOST : GpuErrorCode::NATIVE_ERROR, result}};
    }

    FenceQueryResult VulkanFence::Query() noexcept
    {
        const auto result = device.GetDeviceFn().vkGetFenceStatus(device.GetNativeHandle(), fence);
        if (result == VK_SUCCESS)
            return {FenceStatus::SIGNALED};
        if (result == VK_NOT_READY)
            return {FenceStatus::PENDING};
        return {FenceStatus::FAILED, {result == VK_ERROR_DEVICE_LOST ? GpuErrorCode::DEVICE_LOST : GpuErrorCode::NATIVE_ERROR, result}};
    }

    bool VulkanFence::WaitFor(uint64_t timeoutNs)
    {
        return device.GetDeviceFn().vkWaitForFences(device.GetNativeHandle(), 1, &fence, VK_TRUE, timeoutNs) == VK_SUCCESS;
    }

} // namespace sky::aurora
