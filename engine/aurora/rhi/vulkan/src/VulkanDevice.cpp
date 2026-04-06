//
// Created by blues on 2026/3/29.
//

#include "VulkanDevice.h"
#include "VulkanInstance.h"
#include "VulkanCommandPool.h"
#include "VulkanFence.h"
#include "VulkanSemaphore.h"
#include "VulkanConversion.h"
#include <core/logger/Logger.h>
#include <vector>

static const char *TAG = "AuroraVulkan";

static const std::vector<const char*> VALIDATION_LAYERS = {
    "VK_LAYER_KHRONOS_validation"
};

namespace sky::aurora {

    VulkanDevice::VulkanDevice(VulkanInstance &inst)
        : instance(inst)
    {
    }

    VulkanDevice::~VulkanDevice()
    {
        if (device != VK_NULL_HANDLE) {
            vkDestroyDevice(device, nullptr);
        }
    }

    bool VulkanDevice::OnInit(const DeviceInit& init)
    {
        gpu = instance.GetActiveGpu();
        if (gpu == VK_NULL_HANDLE) {
            LOG_E(TAG, "no physical device available");
            return false;
        }

        vkGetPhysicalDeviceProperties2(gpu, &gpuProperties);
        vkGetPhysicalDeviceMemoryProperties(gpu, &memoryProperties);

        QueryDeviceFeatures();

        if (!CreateDevice()) {
            return false;
        }

        capability.maxThreads = init.parallelContextNum;

        LOG_I(TAG, "VkDevice created on GPU: %s", gpuProperties.properties.deviceName);
        return true;
    }

    std::string VulkanDevice::GetDeviceInfo() const
    {
        return gpuProperties.properties.deviceName;
    }

    void VulkanDevice::WaitIdle() const
    {
        if (device != VK_NULL_HANDLE) {
            vkDeviceWaitIdle(device);
        }
    }

    bool VulkanDevice::CreateDevice()
    {
        // enumerate queue families
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queueFamilyCount, nullptr);
        if (queueFamilyCount == 0) {
            LOG_E(TAG, "no queue family found");
            return false;
        }

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queueFamilyCount, queueFamilies.data());

        // find dedicated queue families
        graphicsQueueFamily = UINT32_MAX;
        computeQueueFamily  = UINT32_MAX;
        transferQueueFamily = UINT32_MAX;

        for (uint32_t i = 0; i < queueFamilyCount; ++i) {
            const auto &props = queueFamilies[i];
            LOG_I(TAG, "queue family %u: count=%u flags=0x%x", i, props.queueCount, props.queueFlags);

            if ((props.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0 && graphicsQueueFamily == UINT32_MAX) {
                graphicsQueueFamily = i;
            }
            // prefer a dedicated compute queue (no graphics bit)
            if ((props.queueFlags & VK_QUEUE_COMPUTE_BIT) != 0 && (props.queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0 && computeQueueFamily == UINT32_MAX) {
                computeQueueFamily = i;
            }
            // prefer a dedicated transfer queue (no graphics or compute bit)
            if ((props.queueFlags & VK_QUEUE_TRANSFER_BIT) != 0 && (props.queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0 &&
                (props.queueFlags & VK_QUEUE_COMPUTE_BIT) == 0 && transferQueueFamily == UINT32_MAX) {
                transferQueueFamily = i;
            }
        }

        // fallback: use graphics queue family for compute / transfer
        if (computeQueueFamily == UINT32_MAX) {
            computeQueueFamily = graphicsQueueFamily;
        }
        if (transferQueueFamily == UINT32_MAX) {
            transferQueueFamily = graphicsQueueFamily;
        }

        if (graphicsQueueFamily == UINT32_MAX) {
            LOG_E(TAG, "no graphics queue family found");
            return false;
        }

        // build unique queue create infos
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        float queuePriority = 1.0f;

        auto addQueue = [&](uint32_t family) {
            for (const auto &info : queueCreateInfos) {
                if (info.queueFamilyIndex == family) {
                    return;
                }
            }
            VkDeviceQueueCreateInfo queueInfo = {};
            queueInfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueInfo.queueFamilyIndex = family;
            queueInfo.queueCount       = 1;
            queueInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueInfo);
        };

        addQueue(graphicsQueueFamily);
        addQueue(computeQueueFamily);
        addQueue(transferQueueFamily);

        // device extensions
        enabledExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

        QueryDeviceFeatures();

        VkDeviceCreateInfo createInfo      = {};
        createInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.pNext                   = &vkFeature11;
        createInfo.queueCreateInfoCount    = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos       = queueCreateInfos.data();
        createInfo.enabledExtensionCount   = static_cast<uint32_t>(enabledExtensions.size());
        createInfo.ppEnabledExtensionNames = enabledExtensions.data();
        createInfo.pEnabledFeatures        = &gpuFeatures.features;

        if (instance.IsDebugEnabled()) {
            createInfo.enabledLayerCount   = static_cast<uint32_t>(VALIDATION_LAYERS.size());
            createInfo.ppEnabledLayerNames = VALIDATION_LAYERS.data();
        }

        VkResult result = vkCreateDevice(gpu, &createInfo, nullptr, &device);
        if (result != VK_SUCCESS) {
            LOG_E(TAG, "failed to create VkDevice, error: %d", result);
            return false;
        }

        // retrieve queues
        vkGetDeviceQueue(device, graphicsQueueFamily, 0, &graphicsQueue);
        vkGetDeviceQueue(device, computeQueueFamily,  0, &computeQueue);
        vkGetDeviceQueue(device, transferQueueFamily, 0, &transferQueue);

        return true;
    }

    void VulkanDevice::QueryDeviceFeatures()
    {
        // query supported features via pNext chain
        gpuFeatures.pNext = &vkFeature11;
        vkFeature11.pNext = &vkFeature12;
        vkFeature12.pNext = &vkFeature13;
        vkFeature13.pNext = &vkFeature14;
        vkGetPhysicalDeviceFeatures2(gpu, &gpuFeatures);
    }

    uint32_t VulkanDevice::GetQueueFamilyIndex(QueueType type) const
    {
        switch (type) {
        case QueueType::COMPUTE:  return computeQueueFamily;
        case QueueType::TRANSFER: return transferQueueFamily;
        default:                  return graphicsQueueFamily;
        }
    }

    Fence *VulkanDevice::CreateFence(const Fence::Descriptor &desc)
    {
        auto *fence = new VulkanFence(*this);
        if (!fence->Init(desc)) {
            delete fence;
            return nullptr;
        }
        return fence;
    }

    Semaphore *VulkanDevice::CreateSema(const Semaphore::Descriptor &desc)
    {
        auto *semaphore = new VulkanSemaphore(*this);
        if (!semaphore->Init(desc)) {
            delete semaphore;
            return nullptr;
        }
        return semaphore;
    }

    Buffer *VulkanDevice::CreateBuffer(const Buffer::Descriptor &desc)
    {
        auto *buf = new VulkanBuffer(*this);
        if (!buf->Init(desc)) {
            delete buf;
            return nullptr;
        }
        return buf;
    }

    Image *VulkanDevice::CreateImage(const Image::Descriptor &desc)
    {
        auto *img = new VulkanImage(*this);
        if (!img->Init(desc)) {
            delete img;
            return nullptr;
        }
        return img;
    }

    Sampler *VulkanDevice::CreateSampler(const Sampler::Descriptor &desc)
    {
        auto *smp = new VulkanSampler(*this);
        if (!smp->Init(desc)) {
            delete smp;
            return nullptr;
        }
        return smp;
    }

    PixelFormatFeatureFlags VulkanDevice::GetFormatFeatureFlags(PixelFormat format) const
    {
        VkFormatProperties props = {};
        vkGetPhysicalDeviceFormatProperties(gpu, FromPixelFormat(format), &props);

        const VkFormatFeatureFlags f = props.optimalTilingFeatures;
        PixelFormatFeatureFlags result;

        if (f & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) {
            result |= PixelFormatFeatureFlagBit::COLOR;
        }
        if (f & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT) {
            result |= PixelFormatFeatureFlagBit::BLEND;
        }
        if (f & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
            result |= PixelFormatFeatureFlagBit::DEPTH_STENCIL;
        }
        if (f & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) {
            result |= PixelFormatFeatureFlagBit::SAMPLE;
        }
        if (f & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) {
            result |= PixelFormatFeatureFlagBit::SAMPLE_FILTER;
        }
        if (f & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) {
            result |= PixelFormatFeatureFlagBit::STORAGE;
        }
        if (f & VK_FORMAT_FEATURE_STORAGE_IMAGE_ATOMIC_BIT) {
            result |= PixelFormatFeatureFlagBit::STORAGE_ATOMIC;
        }

        return result;
    }

    ThreadContext* VulkanDevice::CreateAsyncContext()
    {
        return new VulkanContext(*this);
    }

    void VulkanContext::OnAttach(uint32_t threadIndex)
    {
        pool = std::make_unique<VulkanCommandPool>(device, device.GetQueueFamilyIndex(QueueType::GRAPHICS));
        pool->Init();
    }

    void VulkanContext::OnDetach()
    {
        pool = nullptr;
    }

} // namespace sky::aurora
