//
// Created on 2026/04/07.
//

#include "VulkanFunctions.h"

#if defined(_WIN32)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
#elif defined(__APPLE__)
    #include <dlfcn.h>
#else
    #include <dlfcn.h>
#endif

namespace sky::aurora {

    static void *g_vulkanLibrary = nullptr;

    bool LoadVulkanLibrary(VulkanGlobalFunctions &fn)
    {
#if defined(_WIN32)
        g_vulkanLibrary = LoadLibraryA("vulkan-1.dll");
#elif defined(__APPLE__)
        g_vulkanLibrary = dlopen("libMoltenVK.dylib", RTLD_NOW | RTLD_LOCAL);
        if (g_vulkanLibrary == nullptr) {
            g_vulkanLibrary = dlopen("libvulkan.1.dylib", RTLD_NOW | RTLD_LOCAL);
        }
#elif defined(__ANDROID__)
        g_vulkanLibrary = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
#else
        g_vulkanLibrary = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
        if (g_vulkanLibrary == nullptr) {
            g_vulkanLibrary = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
        }
#endif
        if (g_vulkanLibrary == nullptr) {
            return false;
        }

#if defined(_WIN32)
        fn.vkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
            GetProcAddress(static_cast<HMODULE>(g_vulkanLibrary), "vkGetInstanceProcAddr"));
#else
        fn.vkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
            dlsym(g_vulkanLibrary, "vkGetInstanceProcAddr"));
#endif
        if (fn.vkGetInstanceProcAddr == nullptr) {
            UnloadVulkanLibrary();
            return false;
        }

        // Load global-level functions via vkGetInstanceProcAddr(VK_NULL_HANDLE, ...)
        const auto gipa = fn.vkGetInstanceProcAddr;
        fn.vkEnumerateInstanceVersion             = reinterpret_cast<PFN_vkEnumerateInstanceVersion>(gipa(VK_NULL_HANDLE, "vkEnumerateInstanceVersion"));
        fn.vkEnumerateInstanceExtensionProperties = reinterpret_cast<PFN_vkEnumerateInstanceExtensionProperties>(gipa(VK_NULL_HANDLE, "vkEnumerateInstanceExtensionProperties"));
        fn.vkCreateInstance                       = reinterpret_cast<PFN_vkCreateInstance>(gipa(VK_NULL_HANDLE, "vkCreateInstance"));

        return true;
    }

    void UnloadVulkanLibrary()
    {
        if (g_vulkanLibrary != nullptr) {
#if defined(_WIN32)
            FreeLibrary(static_cast<HMODULE>(g_vulkanLibrary));
#else
            dlclose(g_vulkanLibrary);
#endif
            g_vulkanLibrary = nullptr;
        }
    }

    void LoadInstanceFunctions(PFN_vkGetInstanceProcAddr gipa,
                               VkInstance instance,
                               VulkanInstanceFunctions &fn)
    {
#define LOAD_INSTANCE(name) fn.name = reinterpret_cast<PFN_##name>(gipa(instance, #name))
        LOAD_INSTANCE(vkDestroyInstance);
        LOAD_INSTANCE(vkEnumeratePhysicalDevices);
        LOAD_INSTANCE(vkGetPhysicalDeviceProperties);
        LOAD_INSTANCE(vkGetPhysicalDeviceProperties2);
        LOAD_INSTANCE(vkGetPhysicalDeviceMemoryProperties);
        LOAD_INSTANCE(vkGetPhysicalDeviceQueueFamilyProperties);
        LOAD_INSTANCE(vkGetPhysicalDeviceFeatures2);
        LOAD_INSTANCE(vkGetPhysicalDeviceFormatProperties);
        LOAD_INSTANCE(vkCreateDevice);
        LOAD_INSTANCE(vkGetDeviceProcAddr);

        // optional debug utils
        LOAD_INSTANCE(vkCreateDebugUtilsMessengerEXT);
        LOAD_INSTANCE(vkDestroyDebugUtilsMessengerEXT);
#undef LOAD_INSTANCE
    }

    void LoadDeviceFunctions(PFN_vkGetDeviceProcAddr gdpa,
                             VkDevice device,
                             VulkanDeviceFunctions &fn)
    {
#define LOAD_DEVICE(name) fn.name = reinterpret_cast<PFN_##name>(gdpa(device, #name))
        LOAD_DEVICE(vkDestroyDevice);
        LOAD_DEVICE(vkDeviceWaitIdle);
        LOAD_DEVICE(vkGetDeviceQueue);

        LOAD_DEVICE(vkCreateFence);
        LOAD_DEVICE(vkDestroyFence);
        LOAD_DEVICE(vkWaitForFences);
        LOAD_DEVICE(vkResetFences);

        LOAD_DEVICE(vkCreateSemaphore);
        LOAD_DEVICE(vkDestroySemaphore);
        LOAD_DEVICE(vkGetSemaphoreCounterValue);
        LOAD_DEVICE(vkWaitSemaphores);
        LOAD_DEVICE(vkSignalSemaphore);

        LOAD_DEVICE(vkCreateCommandPool);
        LOAD_DEVICE(vkDestroyCommandPool);
        LOAD_DEVICE(vkResetCommandPool);
        LOAD_DEVICE(vkAllocateCommandBuffers);
        LOAD_DEVICE(vkFreeCommandBuffers);
        LOAD_DEVICE(vkBeginCommandBuffer);
        LOAD_DEVICE(vkEndCommandBuffer);

        LOAD_DEVICE(vkCreateShaderModule);
        LOAD_DEVICE(vkDestroyShaderModule);

        LOAD_DEVICE(vkCreateSampler);
        LOAD_DEVICE(vkDestroySampler);

        LOAD_DEVICE(vkCreateGraphicsPipelines);
        LOAD_DEVICE(vkCreateComputePipelines);
        LOAD_DEVICE(vkDestroyPipeline);

        LOAD_DEVICE(vkDestroyImage);
#undef LOAD_DEVICE
    }

} // namespace sky::aurora
