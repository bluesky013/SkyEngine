#include "ohos/VulkanSurface.h"

#include <VulkanFunctions.h>
#include <core/logger/Logger.h>

#include <algorithm>
#include <cstring>

namespace sky::aurora {

    static const char *TAG = "AuroraVulkan";

    bool EnableOHOSSurfaceExtension(const std::vector<VkExtensionProperties> &supported, std::vector<const char *> &enabled)
    {
        for (const char *name : {VK_KHR_SURFACE_EXTENSION_NAME, VK_OHOS_SURFACE_EXTENSION_NAME}) {
            if (std::none_of(supported.begin(), supported.end(),
                             [name](const auto &extension) { return std::strcmp(extension.extensionName, name) == 0; })) {
                LOG_E(TAG, "HarmonyOS presentation requires %s", name);
                return false;
            }
        }
        enabled.push_back(VK_OHOS_SURFACE_EXTENSION_NAME);
        return true;
    }

    bool CreateOHOSSurface(const VulkanInstanceFunctions &functions, VkInstance instance, void *window, VkSurfaceKHR &surface)
    {
        if (functions.vkCreateSurfaceOHOS == nullptr) {
            LOG_E(TAG, "vkCreateSurfaceOHOS not loaded");
            return false;
        }
        VkSurfaceCreateInfoOHOS info = {};
        info.sType                   = VK_STRUCTURE_TYPE_SURFACE_CREATE_INFO_OHOS;
        info.window                  = static_cast<OHNativeWindow *>(window);
        const VkResult result        = functions.vkCreateSurfaceOHOS(instance, &info, nullptr, &surface);
        if (result != VK_SUCCESS) {
            LOG_E(TAG, "vkCreateSurfaceOHOS failed: %d", result);
            return false;
        }
        return true;
    }

} // namespace sky::aurora
