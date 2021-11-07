//
// Created by Zach Lee on 2021/11/7.
//

#include "vulkan/Device.h"
#include "vulkan/Basic.h"
#include "vulkan/Driver.h"
#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"
#include "core/logger/Logger.h"
#include <vector>

static const char* TAG = "Driver";
const std::vector<const char*> DEVICE_EXTS = {
    "VK_KHR_swapchain"
};

const std::vector<const char*> VALIDATION_LAYERS = {
    "VK_LAYER_KHRONOS_validation"
};

namespace sky::drv {

    Device::Device(Driver& drv) : driver(drv), phyDev(VK_NULL_HANDLE), device(VK_NULL_HANDLE), allocator(VK_NULL_HANDLE)
    {
    }

    Device::~Device()
    {
        if (allocator != VK_NULL_HANDLE) {
            vmaDestroyAllocator(allocator);
        }

        if (device != VK_NULL_HANDLE) {
            vkDestroyDevice(device, VKL_ALLOC);
        }
    }

    bool Device::Init(const Descriptor& des, bool enableDebug)
    {
        auto instance = driver.GetInstance();

        uint32_t count = 0;

        // Pick Physical Device
        vkEnumeratePhysicalDevices(instance, &count, nullptr);
        if (count == 0) {
            return false;
        }

        std::vector<VkPhysicalDevice> phyDevices(count);
        vkEnumeratePhysicalDevices(instance, &count, phyDevices.data());

        uint32_t i = 0;
        for (; i < count; ++i) {
            auto pDev = phyDevices[i];
            vkGetPhysicalDeviceFeatures(pDev, &phyFeatures);
            vkGetPhysicalDeviceProperties(pDev, &phyProps);

            if (phyProps.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                break;
            }
        }
        phyDev = phyDevices[i >= count ? 0 : i];

        count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(phyDev, &count, nullptr);
        if (count == 0) {
            return false;
        }

        queueFamilies.resize(count);
        vkGetPhysicalDeviceQueueFamilyProperties(phyDev, &count, queueFamilies.data());

        // CreateDevice Device
        float queuePriority = 1.0f;
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        VkDeviceQueueCreateInfo queueInfo = {};
        queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        for (i = 0; i < count; ++i) {
            queueInfo.queueFamilyIndex = i;
            queueInfo.queueCount = 1;
            queueInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.emplace_back(queueInfo);
            LOG_I(TAG, "queue family index %u, count %u, flags %u", i, queueFamilies[i].queueCount, queueFamilies[i].queueFlags);
        }

        VkPhysicalDeviceFeatures deviceFeatures{};
        deviceFeatures.multiViewport &= phyFeatures.multiViewport;
        deviceFeatures.samplerAnisotropy &= phyFeatures.samplerAnisotropy;

        VkDeviceCreateInfo devInfo = {};
        devInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

        devInfo.pEnabledFeatures = &deviceFeatures;
        devInfo.enabledExtensionCount = (uint32_t)DEVICE_EXTS.size();
        devInfo.ppEnabledExtensionNames = DEVICE_EXTS.data();

        devInfo.queueCreateInfoCount = (uint32_t)queueCreateInfos.size();
        devInfo.pQueueCreateInfos = queueCreateInfos.data();

        if (enableDebug) {
            devInfo.enabledLayerCount = (uint32_t)VALIDATION_LAYERS.size();
            devInfo.ppEnabledLayerNames = VALIDATION_LAYERS.data();
        }

        VkResult rst = vkCreateDevice(phyDev, &devInfo, VKL_ALLOC, &device);
        if (rst != VK_SUCCESS) {
            LOG_E(TAG, "create device failed -%d", rst);
            return false;
        }

        VmaAllocatorCreateInfo allocatorInfo = {};
        allocatorInfo.device = device;;
        allocatorInfo.physicalDevice = phyDev;
        allocatorInfo.instance = driver.GetInstance();
        allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_1;
        rst = vmaCreateAllocator(&allocatorInfo, &allocator);
        if (rst != VK_SUCCESS) {
            LOG_E(TAG, "create allocator failed -%d", rst);
            return false;
        }

        return true;
    }

    VmaAllocator Device::GetAllocator() const
    {
        return allocator;
    }

}

