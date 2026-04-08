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
    #include <cstdlib>
    #include <dlfcn.h>
    #include <string>
    #include <unistd.h>
#else
    #include <dlfcn.h>
#endif

namespace sky::aurora {

    static void *g_vulkanLibrary = nullptr;

#if defined(__APPLE__)
    namespace {
        bool PathExists(const char *path)
        {
            return path != nullptr && access(path, F_OK) == 0;
        }

        std::string GetModuleDirectory()
        {
            Dl_info info = {};
            if (dladdr(reinterpret_cast<const void *>(&LoadVulkanLibrary), &info) == 0 || info.dli_fname == nullptr) {
                return {};
            }

            std::string modulePath = info.dli_fname;
            const auto slash = modulePath.find_last_of('/');
            if (slash == std::string::npos) {
                return {};
            }
            return modulePath.substr(0, slash);
        }

        void ConfigureBundledVulkanManifests()
        {
            const auto moduleDir = GetModuleDirectory();
            if (moduleDir.empty()) {
                return;
            }

            if (std::getenv("VK_ICD_FILENAMES") == nullptr) {
                const auto icdManifest = moduleDir + "/vulkan/icd.d/MoltenVK_icd.json";
                if (PathExists(icdManifest.c_str())) {
                    setenv("VK_ICD_FILENAMES", icdManifest.c_str(), 0);
                }
            }

            if (std::getenv("VK_LAYER_PATH") == nullptr) {
                const auto layerDir = moduleDir + "/vulkan/explicit_layer.d";
                if (PathExists(layerDir.c_str())) {
                    setenv("VK_LAYER_PATH", layerDir.c_str(), 0);
                }
            }
        }
    }
#endif

    bool LoadVulkanLibrary(VulkanGlobalFunctions &fn)
    {
#if defined(_WIN32)
        g_vulkanLibrary = LoadLibraryA("vulkan-1.dll");
#elif defined(__APPLE__)
        ConfigureBundledVulkanManifests();
    g_vulkanLibrary = dlopen("libvulkan.1.dylib", RTLD_NOW | RTLD_LOCAL);
        if (g_vulkanLibrary == nullptr) {
        g_vulkanLibrary = dlopen("libvulkan.dylib", RTLD_NOW | RTLD_LOCAL);
        }
        if (g_vulkanLibrary == nullptr) {
        g_vulkanLibrary = dlopen("libMoltenVK.dylib", RTLD_NOW | RTLD_LOCAL);
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
        fn.vkEnumerateInstanceLayerProperties     = reinterpret_cast<PFN_vkEnumerateInstanceLayerProperties>(gipa(VK_NULL_HANDLE, "vkEnumerateInstanceLayerProperties"));
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

        LOAD_DEVICE(vkQueueSubmit);

        // dynamic rendering (Vulkan 1.3+)
        LOAD_DEVICE(vkCmdBeginRendering);
        LOAD_DEVICE(vkCmdEndRendering);

        // draw commands
        LOAD_DEVICE(vkCmdDraw);
        LOAD_DEVICE(vkCmdDrawIndexed);
        LOAD_DEVICE(vkCmdDrawIndirect);
        LOAD_DEVICE(vkCmdDrawIndexedIndirect);

        // compute
        LOAD_DEVICE(vkCmdDispatch);
        LOAD_DEVICE(vkCmdDispatchIndirect);

        // state
        LOAD_DEVICE(vkCmdBindPipeline);
        LOAD_DEVICE(vkCmdBindDescriptorSets);
        LOAD_DEVICE(vkCmdBindVertexBuffers);
        LOAD_DEVICE(vkCmdBindIndexBuffer);
        LOAD_DEVICE(vkCmdSetViewport);
        LOAD_DEVICE(vkCmdSetScissor);

        // transfer
        LOAD_DEVICE(vkCmdCopyBuffer);
        LOAD_DEVICE(vkCmdCopyBufferToImage);
        LOAD_DEVICE(vkCmdCopyImageToBuffer);
        LOAD_DEVICE(vkCmdBlitImage);
        LOAD_DEVICE(vkCmdResolveImage);

        // synchronization
        LOAD_DEVICE(vkCmdPipelineBarrier2);
#undef LOAD_DEVICE
    }

} // namespace sky::aurora
