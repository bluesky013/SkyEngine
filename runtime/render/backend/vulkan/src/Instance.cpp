//
// Created by Zach Lee on 2021/11/7.
//

#include <vulkan/Instance.h>
#include <vulkan/vulkan_core.h>
#include <core/logger/Logger.h>

#include <rhi/Util.h>

#ifdef SKY_ENABLE_XR
#include <openxr/openxr_platform.h>
#endif

#include <vector>

static const char *TAG = "Vulkan";

#define SKY_VK_API_VERSION_MAJOR(version) (((uint32_t)(version) >> 22) & 0x7FU)
#define SKY_VK_API_VERSION_MINOR(version) (((uint32_t)(version) >> 12) & 0x3FFU)
#define SKY_VK_API_VERSION_PATCH(version) ((uint32_t)(version) & 0xFFFU)

namespace sky::vk {

    static VkResult CreateDebugUtilsMessengerEXT(VkInstance                                instance,
                                                 const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
                                                 const VkAllocationCallbacks              *pAllocator,
                                                 VkDebugUtilsMessengerEXT                 *pDebugMessenger)
    {
        auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
        if (func != nullptr) {
            return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
        }
        return VK_ERROR_EXTENSION_NOT_PRESENT;
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

    Instance *Instance::Create(const Descriptor &des)
    {
        auto *instance = new Instance();
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
        auto *device = new Device(*this);
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
        uint32_t count;
        vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);
        std::vector<VkExtensionProperties> supported(count);
        vkEnumerateInstanceExtensionProperties(nullptr, &count, supported.data());

        auto extensions = GetInstanceExtensions();

#ifdef SKY_ENABLE_XR
        xrInterface = des.xrInterface;
        std::vector<char> extensionNames;
        if (xrInterface != nullptr) {
            PFN_xrGetVulkanInstanceExtensionsKHR func = reinterpret_cast<PFN_xrGetVulkanInstanceExtensionsKHR>(xrInterface->GetFunction("xrGetVulkanInstanceExtensionsKHR"));
            uint32_t extensionNamesSize = 0;
            func(xrInterface->GetXrInstanceHandle(), xrInterface->GetXrSystemId(), 0, &extensionNamesSize, nullptr);
            extensionNames.resize(extensionNamesSize);
            func(xrInterface->GetXrInstanceHandle(), xrInterface->GetXrSystemId(), extensionNamesSize, &extensionNamesSize, extensionNames.data());

            std::vector<const char*> xrExtensions = rhi::ParseExtensionString(extensionNames.data());
            for (auto &ext : xrExtensions) {
                auto iter = std::find_if(supported.begin(), supported.end(), [ext](const auto &prop) {
                    return strcmp(ext, prop.extensionName) == 0;
                });
                if (iter != supported.end()) {
                    extensions.emplace_back(ext);
                }
            }
        }
#endif

        uint32_t version = 0;
        vkEnumerateInstanceVersion(&version);
        majorVersion = SKY_VK_API_VERSION_MAJOR(version);
        minorVersion = SKY_VK_API_VERSION_MINOR(version);

        LOG_I(TAG, "Vulkan Core apiVersion %u.%u.%u", majorVersion, minorVersion,  SKY_VK_API_VERSION_PATCH(version));

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
            const auto &layers = GetValidationLayers();
            instInfo.enabledLayerCount   = static_cast<uint32_t>(layers.size());
            instInfo.ppEnabledLayerNames = layers.data();

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

        LoadInstance(instance, minorVersion);
        return true;
    }

    VkInstance Instance::GetInstance() const
    {
        return instance;
    }
} // namespace sky::vk
