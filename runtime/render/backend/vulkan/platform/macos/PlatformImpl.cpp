//
// Created by blues on 2024/3/12.
//

#include <vulkan/Swapchain.h>

namespace sky::vk {

    std::vector<const char *> INSTANCE_EXTENSIONS = {
        "VK_KHR_surface",
        "VK_KHR_portability_enumeration",
        "VK_MVK_macos_surface",
        "VK_EXT_metal_surface",
#if _DEBUG
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
#endif
    };
    const std::vector<const char *> DEVICE_EXTENSIONS = {"VK_KHR_swapchain", "VK_KHR_portability_subset"};
    const std::vector<const char *> VALIDATION_LAYERS = {"VK_LAYER_KHRONOS_validation"};

    const std::vector<const char *> &GetInstanceExtensions()
    {
        return INSTANCE_EXTENSIONS;
    }

    const std::vector<const char *> &GetValidationLayers()
    {
        return VALIDATION_LAYERS;
    }

    const std::vector<const char *> &GetDeviceExtensions()
    {
        return DEVICE_EXTENSIONS;
    }

} // namespace sky::vk