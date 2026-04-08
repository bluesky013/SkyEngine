//
// Created on 2026/04/07.
//

#pragma once

#ifndef VK_NO_PROTOTYPES
    #define VK_NO_PROTOTYPES
#endif
#include <vulkan/vulkan.h>

namespace sky::aurora {

    // ---------------------------------------------------------------------------
    // Global-level function pointers (loaded from the Vulkan loader library).
    // These are available before any VkInstance exists.
    // ---------------------------------------------------------------------------
    struct VulkanGlobalFunctions {
        PFN_vkGetInstanceProcAddr                  vkGetInstanceProcAddr                  = nullptr;
        PFN_vkEnumerateInstanceVersion             vkEnumerateInstanceVersion             = nullptr;
        PFN_vkEnumerateInstanceExtensionProperties vkEnumerateInstanceExtensionProperties = nullptr;
        PFN_vkEnumerateInstanceLayerProperties     vkEnumerateInstanceLayerProperties     = nullptr;
        PFN_vkCreateInstance                       vkCreateInstance                       = nullptr;
    };

    // ---------------------------------------------------------------------------
    // Instance-level function pointers (loaded after vkCreateInstance).
    // ---------------------------------------------------------------------------
    struct VulkanInstanceFunctions {
        PFN_vkDestroyInstance                              vkDestroyInstance                              = nullptr;
        PFN_vkEnumeratePhysicalDevices                     vkEnumeratePhysicalDevices                     = nullptr;
        PFN_vkGetPhysicalDeviceProperties                  vkGetPhysicalDeviceProperties                  = nullptr;
        PFN_vkGetPhysicalDeviceProperties2                 vkGetPhysicalDeviceProperties2                 = nullptr;
        PFN_vkGetPhysicalDeviceMemoryProperties            vkGetPhysicalDeviceMemoryProperties            = nullptr;
        PFN_vkGetPhysicalDeviceQueueFamilyProperties       vkGetPhysicalDeviceQueueFamilyProperties       = nullptr;
        PFN_vkGetPhysicalDeviceFeatures2                   vkGetPhysicalDeviceFeatures2                   = nullptr;
        PFN_vkGetPhysicalDeviceFormatProperties            vkGetPhysicalDeviceFormatProperties            = nullptr;
        PFN_vkCreateDevice                                 vkCreateDevice                                 = nullptr;
        PFN_vkGetDeviceProcAddr                            vkGetDeviceProcAddr                            = nullptr;

        // debug utils (optional extension)
        PFN_vkCreateDebugUtilsMessengerEXT                 vkCreateDebugUtilsMessengerEXT                 = nullptr;
        PFN_vkDestroyDebugUtilsMessengerEXT                vkDestroyDebugUtilsMessengerEXT                = nullptr;
    };

    // ---------------------------------------------------------------------------
    // Device-level function pointers (loaded after vkCreateDevice).
    // ---------------------------------------------------------------------------
    struct VulkanDeviceFunctions {
        PFN_vkDestroyDevice                vkDestroyDevice                = nullptr;
        PFN_vkDeviceWaitIdle               vkDeviceWaitIdle               = nullptr;
        PFN_vkGetDeviceQueue               vkGetDeviceQueue               = nullptr;

        // fence
        PFN_vkCreateFence                  vkCreateFence                  = nullptr;
        PFN_vkDestroyFence                 vkDestroyFence                 = nullptr;
        PFN_vkWaitForFences                vkWaitForFences                = nullptr;
        PFN_vkResetFences                  vkResetFences                  = nullptr;

        // semaphore
        PFN_vkCreateSemaphore              vkCreateSemaphore              = nullptr;
        PFN_vkDestroySemaphore             vkDestroySemaphore             = nullptr;
        PFN_vkGetSemaphoreCounterValue     vkGetSemaphoreCounterValue     = nullptr;
        PFN_vkWaitSemaphores               vkWaitSemaphores               = nullptr;
        PFN_vkSignalSemaphore              vkSignalSemaphore              = nullptr;

        // command pool / buffer
        PFN_vkCreateCommandPool            vkCreateCommandPool            = nullptr;
        PFN_vkDestroyCommandPool           vkDestroyCommandPool           = nullptr;
        PFN_vkResetCommandPool             vkResetCommandPool             = nullptr;
        PFN_vkAllocateCommandBuffers       vkAllocateCommandBuffers       = nullptr;
        PFN_vkFreeCommandBuffers           vkFreeCommandBuffers           = nullptr;
        PFN_vkBeginCommandBuffer           vkBeginCommandBuffer           = nullptr;
        PFN_vkEndCommandBuffer             vkEndCommandBuffer             = nullptr;

        // shader
        PFN_vkCreateShaderModule           vkCreateShaderModule           = nullptr;
        PFN_vkDestroyShaderModule          vkDestroyShaderModule          = nullptr;

        // sampler
        PFN_vkCreateSampler                vkCreateSampler                = nullptr;
        PFN_vkDestroySampler               vkDestroySampler               = nullptr;

        // pipeline
        PFN_vkCreateGraphicsPipelines      vkCreateGraphicsPipelines      = nullptr;
        PFN_vkCreateComputePipelines       vkCreateComputePipelines       = nullptr;
        PFN_vkDestroyPipeline              vkDestroyPipeline              = nullptr;

        // image (for non-VMA paths, e.g. swapchain image cleanup)
        PFN_vkDestroyImage                 vkDestroyImage                 = nullptr;

        // queue
        PFN_vkQueueSubmit                  vkQueueSubmit                  = nullptr;

        // dynamic rendering (Vulkan 1.3+)
        PFN_vkCmdBeginRendering            vkCmdBeginRendering            = nullptr;
        PFN_vkCmdEndRendering              vkCmdEndRendering              = nullptr;

        // draw commands
        PFN_vkCmdDraw                      vkCmdDraw                      = nullptr;
        PFN_vkCmdDrawIndexed               vkCmdDrawIndexed               = nullptr;
        PFN_vkCmdDrawIndirect              vkCmdDrawIndirect              = nullptr;
        PFN_vkCmdDrawIndexedIndirect       vkCmdDrawIndexedIndirect       = nullptr;

        // compute
        PFN_vkCmdDispatch                  vkCmdDispatch                  = nullptr;
        PFN_vkCmdDispatchIndirect          vkCmdDispatchIndirect          = nullptr;

        // state
        PFN_vkCmdBindPipeline              vkCmdBindPipeline              = nullptr;
        PFN_vkCmdBindDescriptorSets        vkCmdBindDescriptorSets        = nullptr;
        PFN_vkCmdBindVertexBuffers         vkCmdBindVertexBuffers         = nullptr;
        PFN_vkCmdBindIndexBuffer           vkCmdBindIndexBuffer           = nullptr;
        PFN_vkCmdSetViewport               vkCmdSetViewport               = nullptr;
        PFN_vkCmdSetScissor                vkCmdSetScissor                = nullptr;

        // transfer
        PFN_vkCmdCopyBuffer                vkCmdCopyBuffer                = nullptr;
        PFN_vkCmdCopyBufferToImage         vkCmdCopyBufferToImage         = nullptr;
        PFN_vkCmdCopyImageToBuffer         vkCmdCopyImageToBuffer         = nullptr;
        PFN_vkCmdBlitImage                 vkCmdBlitImage                 = nullptr;
        PFN_vkCmdResolveImage              vkCmdResolveImage              = nullptr;

        // synchronization
        PFN_vkCmdPipelineBarrier2          vkCmdPipelineBarrier2          = nullptr;
    };

    // ---------------------------------------------------------------------------
    // Loader entry point. Loads vkGetInstanceProcAddr from the platform's
    // Vulkan shared library (vulkan-1.dll / libvulkan.so / libMoltenVK.dylib).
    // Returns false if the library cannot be found.
    // ---------------------------------------------------------------------------
    bool LoadVulkanLibrary(VulkanGlobalFunctions &fn);

    // Unloads the platform Vulkan shared library.
    void UnloadVulkanLibrary();

    // Loads instance-level function pointers from the given VkInstance.
    void LoadInstanceFunctions(PFN_vkGetInstanceProcAddr getInstanceProcAddr,
                               VkInstance instance,
                               VulkanInstanceFunctions &fn);

    // Loads device-level function pointers from the given VkDevice.
    void LoadDeviceFunctions(PFN_vkGetDeviceProcAddr getDeviceProcAddr,
                             VkDevice device,
                             VulkanDeviceFunctions &fn);

} // namespace sky::aurora
