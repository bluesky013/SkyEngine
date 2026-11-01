#pragma once

#include <vector>
#include <vulkan/vulkan.h>

namespace sky::aurora {

    struct VulkanInstanceFunctions;

    bool EnableOHOSSurfaceExtension(const std::vector<VkExtensionProperties> &supported, std::vector<const char *> &enabled);
    bool CreateOHOSSurface(const VulkanInstanceFunctions &functions, VkInstance instance, void *window, VkSurfaceKHR &surface);

} // namespace sky::aurora
