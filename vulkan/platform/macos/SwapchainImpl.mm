//
// Created by Zach Lee on 2021/11/28.
//

#include "vulkan/Swapchain.h"
#include <Cocoa/Cocoa.h>
#include <vulkan/Device.h>
#include <vulkan/vulkan_macos.h>

namespace sky::vk {

bool SwapChain::CreateSurface() {
  auto nsWin = static_cast<NSWindow *>(descriptor.window);

  NSBundle* bundle = [NSBundle bundleWithPath: @"/System/Library/Frameworks/QuartzCore.framework"];
  CALayer* layer = [[bundle classNamed: @"CAMetalLayer"] layer];
  NSView* view = nsWin.contentView;
  [view setLayer: layer];
  [view setWantsLayer: YES];

  VkMacOSSurfaceCreateInfoMVK createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_MACOS_SURFACE_CREATE_INFO_MVK;
  createInfo.pView = view;
  createInfo.pNext = nullptr;

  if (vkCreateMacOSSurfaceMVK(device.GetInstance(), &createInfo, nullptr,
                              &surface) != VK_SUCCESS) {
    return false;
  }
  return true;
}

}
