//
// Created by Zach Lee on 2021/11/28.
//

#include "vulkan/Swapchain.h"
#include <core/logger/Logger.h>
#include <vulkan/Basic.h>
#include <vulkan/Device.h>

#include <windows.h>
#include <vulkan/vulkan_win32.h>

static const char *TAG = "Vulkan";

namespace sky::vk {

    bool SwapChain::CreateSurface()
    {
        VkWin32SurfaceCreateInfoKHR surfaceInfo = {};
        surfaceInfo.sType                       = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        surfaceInfo.hwnd                        = (HWND)descriptor.window;
        surfaceInfo.hinstance                   = GetModuleHandle(0);
        VkResult rst                            = vkCreateWin32SurfaceKHR(device.GetInstance(), &surfaceInfo, VKL_ALLOC, &surface);
        if (rst != VK_SUCCESS) {
            LOG_E(TAG, "create win32 surface failed, %d", rst);
            return false;
        }
        return true;
    }

} // namespace sky::vk