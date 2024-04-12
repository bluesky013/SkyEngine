//
// Created by Zach Lee on 2023/6/19.
//

#include <vulkan/Ext.h>

PFN_vkCreateRenderPass2 CreateRenderPass2;
PFN_vkGetPhysicalDeviceFragmentShadingRatesKHR GetPhysicalDeviceFragmentShadingRates;

PFN_vkCmdBlitImage2KHR CmdBlitImage2;
PFN_vkCmdResolveImage2KHR CmdResolveImage2;

void LoadInstance(VkInstance instance, uint32_t minorVersion)
{
    if (minorVersion > 1) {
        CreateRenderPass2 = PFN_vkCreateRenderPass2(vkGetInstanceProcAddr(instance, "vkCreateRenderPass2"));
    } else {
        CreateRenderPass2 = PFN_vkCreateRenderPass2(vkGetInstanceProcAddr(instance, "vkCreateRenderPass2KHR"));
    }

    GetPhysicalDeviceFragmentShadingRates = PFN_vkGetPhysicalDeviceFragmentShadingRatesKHR(vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceFragmentShadingRatesKHR"));
}

void LoadDevice(VkDevice device)
{
    CmdBlitImage2 = (PFN_vkCmdBlitImage2KHR)vkGetDeviceProcAddr(device, "vkCmdBlitImage2");
    CmdResolveImage2 = (PFN_vkCmdResolveImage2KHR)vkGetDeviceProcAddr(device, "vkCmdResolveImage2");
}
