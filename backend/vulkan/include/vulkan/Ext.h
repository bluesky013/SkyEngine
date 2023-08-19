//
// Created by Zach Lee on 2023/6/19.
//

#pragma once

#include <vulkan/vulkan_core.h>

// instance
extern PFN_vkCreateRenderPass2 CreateRenderPass2;
extern PFN_vkGetPhysicalDeviceFragmentShadingRatesKHR GetPhysicalDeviceFragmentShadingRates;

// device
extern PFN_vkCmdBlitImage2KHR CmdBlitImage2;
extern PFN_vkCmdResolveImage2KHR CmdResolveImage2;

void LoadInstance(VkInstance instance, uint32_t minorVersion);
void LoadDevice(VkDevice device);