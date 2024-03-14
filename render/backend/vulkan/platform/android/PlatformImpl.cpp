//
// Created by blues on 2024/3/12.
//

#include <vulkan/Swapchain.h>

namespace sky::vk {

    std::vector<const char *> INSTANCE_EXTENSIONS = {
        "VK_KHR_surface",
        "VK_KHR_android_surface",
    };

    const std::vector<const char *> DEVICE_EXTENSIONS = {"VK_KHR_swapchain"};
    std::vector<const char *> EMPTY;

    const std::vector<const char *> &GetInstanceExtensions()
    {
        return INSTANCE_EXTENSIONS;
    }

    const std::vector<const char *> &GetValidationLayers()
    {
        return EMPTY;
    }

    const std::vector<const char *> &GetDeviceExtensions()
    {
        return DEVICE_EXTENSIONS;
    }
} // namespace sky::vk