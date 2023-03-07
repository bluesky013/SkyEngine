//
// Created by Zach Lee on 2021/11/7.
//

#include "vulkan/Instance.h"
#include "core/logger/Logger.h"
#include <vector>

static const char *TAG = "Vulkan";

namespace sky::vk {

    static VkResult CreateDebugUtilsMessengerEXT(VkInstance                                instance,
                                                 const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
                                                 const VkAllocationCallbacks              *pAllocator,
                                                 VkDebugUtilsMessengerEXT                 *pDebugMessenger)
    {
        auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
        if (func != nullptr) {
            return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
        } else {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
    }

    static void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks *pAllocator)
    {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func != nullptr) {
            func(instance, debugMessenger, pAllocator);
        }
    }


    static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT      messageSeverity,
                                                        VkDebugUtilsMessageTypeFlagsEXT             messageType,
                                                        const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
                                                        void                                       *pUserData)
    {
        LOG_I(TAG, "vulkan debug layer %s", pCallbackData->pMessage);
        return VK_FALSE;
    }

    const std::vector<const char *> extensions = {
        "VK_KHR_surface",
#if _WIN32
        "VK_KHR_win32_surface",
#elif __APPLE__
        "VK_KHR_portability_enumeration",
        "VK_MVK_macos_surface",
        "VK_EXT_metal_surface",
#endif
    };

    const std::vector<const char *> validationLayers = {"VK_LAYER_KHRONOS_validation"};

    Instance *Instance::Create(const Descriptor &des)
    {
        auto instance = new Instance();
        if (!instance->Init(des)) {
            delete instance;
            instance = nullptr;
        }
        return instance;
    }

    void Instance::Destroy(Instance *instance)
    {
        if (instance != nullptr) {
            delete instance;
        }
    }

    Device *Instance::CreateDevice(const Device::Descriptor &des)
    {
        auto device = new Device(*this);
        if (!device->Init(des, debug != VK_NULL_HANDLE)) {
            delete device;
            device = nullptr;
        }
        return device;
    }

    Instance::Instance() : instance(VK_NULL_HANDLE), debug(VK_NULL_HANDLE)
    {
    }

    Instance::~Instance()
    {
        if (debug != VK_NULL_HANDLE) {
            DestroyDebugUtilsMessengerEXT(instance, debug, VKL_ALLOC);
        }

        if (instance != VK_NULL_HANDLE) {
            vkDestroyInstance(instance, VKL_ALLOC);
        }
    }

    bool Instance::Init(const Descriptor &des)
    {
        uint32_t version = 0;
        VkResult result = vkEnumerateInstanceVersion(&version);
        LOG_I(TAG, "Vulkan Core apiVersion %u.%u.%u", VK_API_VERSION_MAJOR(version), VK_API_VERSION_MINOR(version),  VK_API_VERSION_PATCH(version));

        VkApplicationInfo app  = {};
        app.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        app.pApplicationName   = des.appName.c_str();
        app.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        app.pEngineName        = des.engineName.c_str();
        app.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
        app.apiVersion         = version;

        VkInstanceCreateInfo instInfo    = {};
#ifdef __APPLE__
        instInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif
        instInfo.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instInfo.pApplicationInfo        = &app;
        instInfo.enabledExtensionCount   = static_cast<uint32_t>(extensions.size());
        instInfo.ppEnabledExtensionNames = extensions.data();

        VkDebugUtilsMessengerCreateInfoEXT debugInfo = {};
        if (des.enableDebugLayer) {
            instInfo.enabledLayerCount   = static_cast<uint32_t>(validationLayers.size());
            instInfo.ppEnabledLayerNames = validationLayers.data();

            debugInfo.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            debugInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            debugInfo.messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                    VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            debugInfo.pfnUserCallback = DebugCallback;
            instInfo.pNext            = (VkDebugUtilsMessengerCreateInfoEXT *)&debugInfo;
        }

        VkResult rst = vkCreateInstance(&instInfo, VKL_ALLOC, &instance);
        if (rst != VK_SUCCESS) {
            LOG_E(TAG, "create instance failed %u", rst);
            return false;
        }

        if (des.enableDebugLayer) {
            CreateDebugUtilsMessengerEXT(instance, &debugInfo, VKL_ALLOC, &debug);
        }

        InitFunctions();
        PrintSupportedExtensions();
        return true;
    }

    VkInstance Instance::GetInstance() const
    {
        return instance;
    }

    void Instance::InitFunctions()
    {
        getPhysicalDeviceFragmentShadingRate = PFN_vkGetPhysicalDeviceFragmentShadingRatesKHR(vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceFragmentShadingRatesKHR"));
        createRenderPass2                    = PFN_vkCreateRenderPass2(vkGetInstanceProcAddr(instance, "vkCreateRenderPass2"));
    }

    VkResult Instance::GetPhysicalDeviceFragmentShadingRates(VkPhysicalDevice physicalDevice, uint32_t *pFragmentShadingRateCount,
                                                   VkPhysicalDeviceFragmentShadingRateKHR *pFragmentShadingRates)
    {
        return getPhysicalDeviceFragmentShadingRate == nullptr ? VK_ERROR_UNKNOWN :
            getPhysicalDeviceFragmentShadingRate(physicalDevice, pFragmentShadingRateCount, pFragmentShadingRates);
    }

    VkResult Instance::CreateRenderPass2(VkDevice device,
                               const VkRenderPassCreateInfo2* pCreateInfo,
                               const VkAllocationCallbacks* pAllocator,
                               VkRenderPass* pRenderPass)
    {
        return createRenderPass2(device, pCreateInfo, pAllocator, pRenderPass);
    }

    void Instance::PrintSupportedExtensions() const
    {
        uint32_t count;
        vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);
        std::vector<VkExtensionProperties> supported(count);
        vkEnumerateInstanceExtensionProperties(nullptr, &count, supported.data());
        for (auto &ext : supported) {
            LOG_I(TAG, "supported extensions name %s, version %u", ext.extensionName, ext.specVersion);
        }
    }

} // namespace sky::vk
