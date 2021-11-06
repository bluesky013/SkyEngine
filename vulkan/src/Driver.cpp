//
// Created by Zach Lee on 2021/11/7.
//

#include "vulkan/Driver.h"
#include "core/logger/Logger.h"
#include <vector>

static const char* TAG = "Driver";

namespace sky::drv {

    static VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
        const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger)
    {
        auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
        if (func != nullptr) {
            return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
        } else {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
    }

    static void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger,
        const VkAllocationCallbacks* pAllocator)
    {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func != nullptr) {
            func(instance, debugMessenger, pAllocator);
        }
    }

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData)
    {
        return VK_FALSE;
    }

    const std::vector<const char*> extensions = {
        "VK_KHR_surface",
#if _WIN32
        "VK_KHR_win32_surface",
#else
        "VK_MVK_macos_surface",
        "VK_EXT_metal_surface",
#endif
    };

    const std::vector<const char*> validationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };

    Driver* Driver::Create(const Descriptor& des)
    {
        auto driver = new Driver();
        if (!driver->Init(des)) {
            delete driver;
            driver = nullptr;
        }
        return driver;
    }

    void Driver::Destroy(Driver* driver)
    {
        if (driver != nullptr) {
            delete driver;
        }
    }

    Driver::Driver() : instance(VK_NULL_HANDLE), debug(VK_NULL_HANDLE)
    {
    }

    Driver::~Driver()
    {
        if (debug != VK_NULL_HANDLE) {
            DestroyDebugUtilsMessengerEXT(instance, debug, VKL_ALLOC);
        }

        if (instance != VK_NULL_HANDLE) {
            vkDestroyInstance(instance, VKL_ALLOC);
        }
    }

    bool Driver::Init(const Descriptor& des)
    {
        VkApplicationInfo app = {};
        app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        app.pApplicationName = des.appName.c_str();
        app.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        app.pEngineName = des.engineName.c_str();
        app.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        app.apiVersion = VK_API_VERSION_1_1;

        VkInstanceCreateInfo instInfo = {};
        instInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instInfo.pApplicationInfo = &app;
        instInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        instInfo.ppEnabledExtensionNames = extensions.data();

        instInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        instInfo.ppEnabledLayerNames = validationLayers.data();

        VkDebugUtilsMessengerCreateInfoEXT debugInfo = {};
        if (des.enableDebugLayer) {
            debugInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            debugInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            debugInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            debugInfo.pfnUserCallback = debugCallback;
            instInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugInfo;
        }

        VkResult rst = vkCreateInstance(&instInfo, VKL_ALLOC, &instance);
        if (rst != VK_NULL_HANDLE) {
            return false;
        }

        if (des.enableDebugLayer) {
            CreateDebugUtilsMessengerEXT(instance, &debugInfo, VKL_ALLOC, &debug);
        }
        return true;
    }

}