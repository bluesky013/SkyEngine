//
// Created by blues on 2024/3/12.
//

#include <vulkan/Swapchain.h>

namespace sky::vk {

    std::vector<const char *> INSTANCE_EXTENSIONS = {
        "VK_KHR_surface",
        "VK_KHR_android_surface",
#ifdef _DEBUG
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
#endif
    };

    const std::vector<const char *> DEVICE_EXTENSIONS = {
            "VK_KHR_swapchain",
            "VK_KHR_create_renderpass2",
    };
    std::vector<const char *> DEVICE_LAYER = {"VK_LAYER_KHRONOS_validation"};

    const std::vector<const char *> &GetInstanceExtensions()
    {
        return INSTANCE_EXTENSIONS;
    }

    const std::vector<const char *> &GetValidationLayers()
    {
        return DEVICE_LAYER;
    }

    const std::vector<const char *> &GetDeviceExtensions()
    {
        return DEVICE_EXTENSIONS;
    }
} // namespace sky::vk