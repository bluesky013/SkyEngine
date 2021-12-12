//
// Created by Zach Lee on 2021/11/28.
//

#include "vulkan/Swapchain.h"
#include <Cocoa/Cocoa.h>
#include <vulkan/Device.h>
#include <vulkan/vulkan_macos.h>

namespace sky::drv {

    bool SwapChain::CreateSurface(const Descriptor& des)
    {
        auto nsView = static_cast<NSView*>(des.window);

        NSBundle* bundle = [NSBundle bundleWithPath: @"/System/Library/Frameworks/QuartzCore.framework"];
        if (!bundle) {
            return false;
        }
        CALayer* layer = [[bundle classNamed: @"CAMetalLayer"] layer];
        if (!layer) {
            return false;
        }
        [nsView setLayer: layer];
        [nsView setWantsLayer: YES];

        VkMacOSSurfaceCreateInfoMVK createInfo {};
        createInfo.sType = VK_STRUCTURE_TYPE_MACOS_SURFACE_CREATE_INFO_MVK;
        createInfo.pView = nsView;
        createInfo.pNext = nullptr;

        if (vkCreateMacOSSurfaceMVK(device.GetInstance(), &createInfo, nullptr, &surface) != VK_SUCCESS) {
            return false;
        }
        return true;
    }

}