//
// Created by Zach Lee on 2021/11/7.
//

#include "vulkan/Fence.h"
#include "core/logger/Logger.h"
#include "vulkan/Basic.h"
#include "vulkan/Device.h"
static const char *TAG = "Driver";

namespace sky::drv {

    Fence::Fence(Device &dev) : DevObject(dev), fence(VK_NULL_HANDLE)
    {
    }

    Fence::~Fence()
    {
        if (fence != VK_NULL_HANDLE) {
            vkDestroyFence(device.GetNativeHandle(), fence, VKL_ALLOC);
        }
    }

    bool Fence::Init(const Descriptor &des)
    {
        VkFenceCreateInfo fenceInfo = {};
        fenceInfo.sType             = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags             = des.flag;
        auto rst                    = vkCreateFence(device.GetNativeHandle(), &fenceInfo, VKL_ALLOC, &fence);
        if (rst != VK_SUCCESS) {
            LOG_E(TAG, "create fence failed, %d", rst);
            return false;
        }
        return true;
    }

    void Fence::Wait(uint64_t timeout)
    {
        vkWaitForFences(device.GetNativeHandle(), 1, &fence, VK_TRUE, timeout);
    }

    void Fence::Reset()
    {
        vkResetFences(device.GetNativeHandle(), 1, &fence);
    }

    VkFence Fence::GetNativeHandle() const
    {
        return fence;
    }

} // namespace sky::drv