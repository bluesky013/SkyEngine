//
// Created by Zach Lee on 2021/11/28.
//

#include "vulkan/Swapchain.h"
#include <core/logger/Logger.h>
#include <vulkan/Basic.h>
#include <vulkan/Device.h>

#include <vulkan/vulkan_android.h>

static const char *TAG = "Vulkan";

namespace sky::vk {

    bool SwapChain::CreateSurface()
    {
        VkAndroidSurfaceCreateInfoKHR surfaceInfo = {};

        surfaceInfo.sType  = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
        surfaceInfo.window = reinterpret_cast<ANativeWindow *>(descriptor.window);

        VkResult rst                            = vkCreateAndroidSurfaceKHR(device.GetInstance(), &surfaceInfo, VKL_ALLOC, &surface);
        if (rst != VK_SUCCESS) {
            LOG_E(TAG, "create win32 surface failed, %d", rst);
            return false;
        }
        return true;
    }

} // namespace sky::vk