//
// Created by blues on 2026/3/29.
//

#include <VulkanInstance.h>
#include <VulkanDevice.h>
#include <core/logger/Logger.h>
#include <core/platform/Platform.h>
#include <cstring>
#include <vector>

static const char *TAG = "AuroraVulkan";

namespace sky::aurora {

    static bool HasExtension(const std::vector<VkExtensionProperties> &extensions, const char *name)
    {
        for (const auto &extension : extensions) {
            if (std::strcmp(extension.extensionName, name) == 0) {
                return true;
            }
        }
        return false;
    }

    static bool HasLayer(const std::vector<VkLayerProperties> &layers, const char *name)
    {
        for (const auto &layer : layers) {
            if (std::strcmp(layer.layerName, name) == 0) {
                return true;
            }
        }
        return false;
    }

    static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT      messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT             messageType,
        const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
        void                                       *pUserData)
    {
        if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
            LOG_E(TAG, "validation: %s", pCallbackData->pMessage);
        } else if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
            LOG_W(TAG, "validation: %s", pCallbackData->pMessage);
        } else {
            LOG_I(TAG, "validation: %s", pCallbackData->pMessage);
        }
        return VK_FALSE;
    }

    static const std::vector<const char*> VALIDATION_LAYERS = {
        "VK_LAYER_KHRONOS_validation"
    };

    VulkanInstance::~VulkanInstance()
    {
        if (debugMessenger != VK_NULL_HANDLE && instanceFn.vkDestroyDebugUtilsMessengerEXT != nullptr) {
            instanceFn.vkDestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
        }
        if (instance != VK_NULL_HANDLE) {
            instanceFn.vkDestroyInstance(instance, nullptr);
        }
        UnloadVulkanLibrary();
    }

    bool VulkanInstance::Init(const Instance::Descriptor &desc)
    {
        enableDebugLayer = desc.enableDebugLayer;

        if (!LoadVulkanLibrary(globalFn)) {
            LOG_E(TAG, "failed to load Vulkan library");
            return false;
        }

        if (!CreateInstance(desc)) {
            return false;
        }

        LoadInstanceFunctions(globalFn.vkGetInstanceProcAddr, instance, instanceFn);

        if (enableDebugLayer) {
            SetupDebugMessenger();
        }
        if (!PickPhysicalDevice()) {
            return false;
        }
        return true;
    }

    Device *VulkanInstance::CreateDevice()
    {
        auto *device = new VulkanDevice(*this);
        if (!device->Init()) {
            delete device;
            return nullptr;
        }
        return device;
    }

    bool VulkanInstance::CreateInstance(const Instance::Descriptor &desc)
    {
        enabledExtensions.clear();
        enabledLayers.clear();

        // query supported extensions
        uint32_t extCount = 0;
        globalFn.vkEnumerateInstanceExtensionProperties(nullptr, &extCount, nullptr);
        std::vector<VkExtensionProperties> supportedExts(extCount);
        globalFn.vkEnumerateInstanceExtensionProperties(nullptr, &extCount, supportedExts.data());
        LOG_I(TAG, "loader reported %u instance extensions", extCount);
        for (const auto &extension : supportedExts) {
            LOG_I(TAG, "instance extension: %s (spec %u)", extension.extensionName, extension.specVersion);
        }

        uint32_t layerCount = 0;
        std::vector<VkLayerProperties> supportedLayers;
        if (globalFn.vkEnumerateInstanceLayerProperties != nullptr) {
            globalFn.vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
            supportedLayers.resize(layerCount);
            globalFn.vkEnumerateInstanceLayerProperties(&layerCount, supportedLayers.data());
        }
        LOG_I(TAG, "loader reported %u instance layers", layerCount);
        for (const auto &layer : supportedLayers) {
            LOG_I(TAG, "instance layer: %s (spec %u)", layer.layerName, layer.specVersion);
        }

        enabledExtensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
#if SKY_PLATFORM_WINDOWS
        enabledExtensions.push_back("VK_KHR_win32_surface");
#elif SKY_PLATFORM_MACOS || SKY_PLATFORM_IOS
        enabledExtensions.push_back("VK_EXT_metal_surface");
        if (HasExtension(supportedExts, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME)) {
            enabledExtensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
        } else {
            LOG_W(TAG, "instance extension %s is not available; portability enumeration disabled",
                  VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
        }
#elif SKY_PLATFORM_ANDROID
        enabledExtensions.push_back("VK_KHR_android_surface");
#endif

        if (enableDebugLayer) {
            if (HasExtension(supportedExts, VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
                enabledExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            } else {
                LOG_W(TAG, "instance extension %s is not available; debug messenger disabled",
                      VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            }

            for (const char *layerName : VALIDATION_LAYERS) {
                if (HasLayer(supportedLayers, layerName)) {
                    enabledLayers.push_back(layerName);
                } else {
                    LOG_W(TAG, "validation layer %s is not available; continuing without it", layerName);
                }
            }
        }

        for (const char *extension : enabledExtensions) {
            LOG_I(TAG, "enabled instance extension: %s", extension);
        }
        for (const char *layer : enabledLayers) {
            LOG_I(TAG, "enabled instance layer: %s", layer);
        }

        uint32_t apiVersion = 0;
        globalFn.vkEnumerateInstanceVersion(&apiVersion);
        LOG_I(TAG, "Vulkan API version: %u.%u.%u",
              VK_API_VERSION_MAJOR(apiVersion),
              VK_API_VERSION_MINOR(apiVersion),
              VK_API_VERSION_PATCH(apiVersion));

        VkApplicationInfo appInfo  = {};
        appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName   = desc.appName.c_str();
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName        = desc.engineName.c_str();
        appInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion         = apiVersion;

        VkInstanceCreateInfo createInfo    = {};
        createInfo.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo        = &appInfo;
        createInfo.enabledExtensionCount   = static_cast<uint32_t>(enabledExtensions.size());
        createInfo.ppEnabledExtensionNames = enabledExtensions.data();
        createInfo.enabledLayerCount       = static_cast<uint32_t>(enabledLayers.size());
        createInfo.ppEnabledLayerNames     = enabledLayers.data();
#if SKY_PLATFORM_MACOS || SKY_PLATFORM_IOS
        if (HasExtension(supportedExts, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME)) {
            createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
        }
#endif

        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = {};
        if (enableDebugLayer) {
            debugCreateInfo.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
                                              VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
            debugCreateInfo.messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                              VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                              VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            debugCreateInfo.pfnUserCallback = DebugCallback;
            createInfo.pNext                = &debugCreateInfo;
        }

        VkResult result = globalFn.vkCreateInstance(&createInfo, nullptr, &instance);
        if (result != VK_SUCCESS) {
            LOG_E(TAG, "failed to create VkInstance, error: %d", result);
            return false;
        }
        LOG_I(TAG, "VkInstance created successfully");
        return true;
    }

    bool VulkanInstance::SetupDebugMessenger()
    {
        VkDebugUtilsMessengerCreateInfoEXT createInfo = {};
        createInfo.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
                                     VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
        createInfo.messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                     VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                     VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = DebugCallback;

        VkResult result = instanceFn.vkCreateDebugUtilsMessengerEXT != nullptr
            ? instanceFn.vkCreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger)
            : VK_ERROR_EXTENSION_NOT_PRESENT;
        if (result != VK_SUCCESS) {
            LOG_W(TAG, "failed to setup debug messenger, error: %d", result);
            return false;
        }
        LOG_I(TAG, "debug messenger created");
        return true;
    }

    bool VulkanInstance::PickPhysicalDevice()
    {
        uint32_t deviceCount = 0;
        instanceFn.vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
        if (deviceCount == 0) {
            LOG_E(TAG, "no Vulkan-capable GPU found");
            return false;
        }

        physicalDevices.resize(deviceCount);
        instanceFn.vkEnumeratePhysicalDevices(instance, &deviceCount, physicalDevices.data());

        // prefer discrete GPU
        activeGpu = physicalDevices[0];
        for (auto &gpu : physicalDevices) {
            VkPhysicalDeviceProperties props = {};
            instanceFn.vkGetPhysicalDeviceProperties(gpu, &props);
            LOG_I(TAG, "found GPU: %s (type: %u)", props.deviceName, props.deviceType);
            if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                activeGpu = gpu;
            }
        }

        VkPhysicalDeviceProperties activeProps = {};
        instanceFn.vkGetPhysicalDeviceProperties(activeGpu, &activeProps);
        LOG_I(TAG, "selected GPU: %s", activeProps.deviceName);
        return true;
    }

} // namespace sky::aurora

extern "C" SKY_EXPORT sky::aurora::Instance::Impl *CreateInstance()
{
    return new sky::aurora::VulkanInstance();
}
