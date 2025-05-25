//
// Created by Zach Lee on 2021/11/28.
//

#include "vulkan/Swapchain.h"
#include <Cocoa/Cocoa.h>
#include <vulkan/Device.h>
#include <vulkan/vulkan_macos.h>

namespace sky::vk {

    bool SwapChain::CreateSurface()
    {
        auto view = static_cast<NSView *>(descriptor.window);

        NSBundle *bundle = [NSBundle bundleWithPath:@"/System/Library/Frameworks/QuartzCore.framework"];
        CALayer  *layer  = [[bundle classNamed:@"CAMetalLayer"] layer];
        [view setLayer:layer];
        [view setWantsLayer:YES];

        VkMacOSSurfaceCreateInfoMVK createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_MACOS_SURFACE_CREATE_INFO_MVK;
        createInfo.pView = view;
        createInfo.pNext = nullptr;

        if (vkCreateMacOSSurfaceMVK(device.GetInstanceId(), &createInfo, nullptr, &surface) != VK_SUCCESS) {
            return false;
        }
        return true;
    }

}
