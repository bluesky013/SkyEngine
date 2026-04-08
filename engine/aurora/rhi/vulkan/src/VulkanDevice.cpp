//
// Created by blues on 2026/3/29.
//

#include "VulkanDevice.h"
#include "VulkanInstance.h"
#include "VulkanCommandPool.h"
#include "VulkanFence.h"
#include "VulkanSemaphore.h"
#include "VulkanConversion.h"
#include "VulkanPipelineState.h"
#include <core/logger/Logger.h>
#include <cstring>
#include <vector>

static const char *TAG = "AuroraVulkan";
static const char *PORTABILITY_SUBSET_EXTENSION = "VK_KHR_portability_subset";

namespace sky::aurora {

    static bool HasDeviceExtension(const std::vector<VkExtensionProperties> &extensions, const char *name)
    {
        for (const auto &extension : extensions) {
            if (std::strcmp(extension.extensionName, name) == 0) {
                return true;
            }
        }
        return false;
    }

    VulkanDevice::VulkanDevice(VulkanInstance &inst)
        : instance(inst)
    {
    }

    VulkanDevice::~VulkanDevice()
    {
        if (allocator != VK_NULL_HANDLE) {
            vmaDestroyAllocator(allocator);
            allocator = VK_NULL_HANDLE;
        }
        if (device != VK_NULL_HANDLE) {
            deviceFn.vkDestroyDevice(device, nullptr);
        }
    }

    bool VulkanDevice::OnInit(const DeviceInit& init)
    {
        (void)init;
        gpu = instance.GetActiveGpu();
        if (gpu == VK_NULL_HANDLE) {
            LOG_E(TAG, "no physical device available");
            return false;
        }

        const auto &instFn = instance.GetInstanceFn();
        instFn.vkGetPhysicalDeviceProperties2(gpu, &gpuProperties);
        instFn.vkGetPhysicalDeviceMemoryProperties(gpu, &memoryProperties);

        QueryDeviceFeatures();

        if (!CreateDevice()) {
            return false;
        }

        if (!CreateAllocator()) {
            return false;
        }

        LOG_I(TAG, "VkDevice created on GPU: %s", gpuProperties.properties.deviceName);
        return true;
    }

    void VulkanDevice::UpdateDeviceCaps()
    {
        capability.maxThreads = std::max(std::thread::hardware_concurrency(), 1U);
        capability.anisotropyEnable = gpuFeatures.features.samplerAnisotropy == VK_TRUE;

        LOG_I(TAG, "sampler anisotropy: %s", capability.anisotropyEnable ? "enabled" : "disabled");
    }

    std::string VulkanDevice::GetDeviceInfo() const
    {
        return gpuProperties.properties.deviceName;
    }

    void VulkanDevice::WaitIdle() const
    {
        if (device != VK_NULL_HANDLE) {
            deviceFn.vkDeviceWaitIdle(device);
        }
    }

    bool VulkanDevice::CreateDevice()
    {
        const auto &instFn = instance.GetInstanceFn();
        enabledExtensions.clear();

        uint32_t extensionCount = 0;
        instFn.vkEnumerateDeviceExtensionProperties(gpu, nullptr, &extensionCount, nullptr);
        std::vector<VkExtensionProperties> supportedExtensions(extensionCount);
        if (extensionCount > 0) {
            instFn.vkEnumerateDeviceExtensionProperties(gpu, nullptr, &extensionCount, supportedExtensions.data());
        }
        LOG_I(TAG, "device reports %u extensions", extensionCount);
        for (const auto &extension : supportedExtensions) {
            LOG_I(TAG, "device extension: %s (spec %u)", extension.extensionName, extension.specVersion);
        }

        // enumerate queue families
        uint32_t queueFamilyCount = 0;
        instFn.vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queueFamilyCount, nullptr);
        if (queueFamilyCount == 0) {
            LOG_E(TAG, "no queue family found");
            return false;
        }

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        instFn.vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queueFamilyCount, queueFamilies.data());

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
        if (HasDeviceExtension(supportedExtensions, PORTABILITY_SUBSET_EXTENSION)) {
            enabledExtensions.push_back(PORTABILITY_SUBSET_EXTENSION);
        }

        for (const char *extension : enabledExtensions) {
            LOG_I(TAG, "enabled device extension: %s", extension);
        }

        QueryDeviceFeatures();

        if (vkFeature13.dynamicRendering == VK_FALSE) {
            LOG_E(TAG, "selected GPU does not support dynamicRendering, but AuroraVulkan requires vkCmdBeginRendering");
            return false;
        }
        if (vkFeature12.timelineSemaphore == VK_FALSE) {
            LOG_E(TAG, "selected GPU does not support timelineSemaphore, but AuroraVulkan requires timeline semaphores");
            return false;
        }

        VkPhysicalDeviceFeatures enabledCoreFeatures = {};
        enabledCoreFeatures.samplerAnisotropy = gpuFeatures.features.samplerAnisotropy;
        VkPhysicalDeviceVulkan11Features enabledFeature11 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES};
        VkPhysicalDeviceVulkan12Features enabledFeature12 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
        VkPhysicalDeviceVulkan13Features enabledFeature13 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
        VkPhysicalDeviceVulkan14Features enabledFeature14 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES};
        enabledFeature12.timelineSemaphore = VK_TRUE;
        enabledFeature13.dynamicRendering = VK_TRUE;
        enabledFeature11.pNext = &enabledFeature12;
        enabledFeature12.pNext = &enabledFeature13;
        enabledFeature13.pNext = &enabledFeature14;

        VkDeviceCreateInfo createInfo      = {};
        createInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.pNext                   = &enabledFeature11;
        createInfo.queueCreateInfoCount    = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos       = queueCreateInfos.data();
        createInfo.enabledExtensionCount   = static_cast<uint32_t>(enabledExtensions.size());
        createInfo.ppEnabledExtensionNames = enabledExtensions.data();
        createInfo.pEnabledFeatures        = &enabledCoreFeatures;

        VkResult result = instFn.vkCreateDevice(gpu, &createInfo, nullptr, &device);
        if (result != VK_SUCCESS) {
            LOG_E(TAG, "failed to create VkDevice, error: %d", result);
            return false;
        }

        // Load device-level function pointers
        LoadDeviceFunctions(instFn.vkGetDeviceProcAddr, device, deviceFn);

        // retrieve queues
        deviceFn.vkGetDeviceQueue(device, graphicsQueueFamily, 0, &graphicsQueue);
        deviceFn.vkGetDeviceQueue(device, computeQueueFamily,  0, &computeQueue);
        deviceFn.vkGetDeviceQueue(device, transferQueueFamily, 0, &transferQueue);

        return true;
    }

    bool VulkanDevice::CreateAllocator()
    {
        VmaVulkanFunctions vulkanFunctions = {};
        vulkanFunctions.vkGetInstanceProcAddr = instance.GetGlobalFn().vkGetInstanceProcAddr;
        vulkanFunctions.vkGetDeviceProcAddr   = instance.GetInstanceFn().vkGetDeviceProcAddr;

        VmaAllocatorCreateInfo allocatorCI = {};
        allocatorCI.physicalDevice   = gpu;
        allocatorCI.device           = device;
        allocatorCI.instance         = instance.GetNativeHandle();
        allocatorCI.pVulkanFunctions = &vulkanFunctions;
        allocatorCI.vulkanApiVersion = gpuProperties.properties.apiVersion;

        const VkResult res = vmaCreateAllocator(&allocatorCI, &allocator);
        if (res != VK_SUCCESS) {
            LOG_E(TAG, "vmaCreateAllocator failed: %d", res);
            return false;
        }
        return true;
    }

    void VulkanDevice::QueryDeviceFeatures()
    {
        // query supported features via pNext chain
        gpuFeatures.pNext = &vkFeature11;
        vkFeature11.pNext = &vkFeature12;
        vkFeature12.pNext = &vkFeature13;
        vkFeature13.pNext = &vkFeature14;
        instance.GetInstanceFn().vkGetPhysicalDeviceFeatures2(gpu, &gpuFeatures);
    }

    uint32_t VulkanDevice::GetQueueFamilyIndex(QueueType type) const
    {
        switch (type) {
        case QueueType::COMPUTE:  return computeQueueFamily;
        case QueueType::TRANSFER: return transferQueueFamily;
        default:                  return graphicsQueueFamily;
        }
    }

    CommandPool *VulkanDevice::CreateCommandPool(QueueType type)
    {
        auto *pool = new VulkanCommandPool(*this, GetQueueFamilyIndex(type));
        if (!pool->Init()) {
            delete pool;
            return nullptr;
        }
        return pool;
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

    ShaderFunction *VulkanDevice::CreateShaderFunction(const ShaderFunction::Descriptor &desc)
    {
        auto *fn = new VulkanShaderFunction(*this);
        if (!fn->Init(desc)) {
            delete fn;
            return nullptr;
        }
        return fn;
    }

    Shader *VulkanDevice::CreateShader(const Shader::Descriptor &desc)
    {
        auto *shader = new VulkanShader(*this);
        if (!shader->Init(desc)) {
            delete shader;
            return nullptr;
        }
        return shader;
    }

    GraphicsPipeline *VulkanDevice::CreatePipelineState(const GraphicsPipeline::Descriptor &desc)
    {
        auto *pipeline = new VulkanGraphicsPipeline(*this);
        if (!pipeline->Init(desc)) {
            delete pipeline;
            return nullptr;
        }
        return pipeline;
    }

    ComputePipeline *VulkanDevice::CreatePipelineState(const ComputePipeline::Descriptor &desc)
    {
        auto *pipeline = new VulkanComputePipeline(*this);
        if (!pipeline->Init(desc)) {
            delete pipeline;
            return nullptr;
        }
        return pipeline;
    }

    PixelFormatFeatureFlags VulkanDevice::GetFormatFeatureFlags(PixelFormat format) const
    {
        VkFormatProperties props = {};
        instance.GetInstanceFn().vkGetPhysicalDeviceFormatProperties(gpu, FromPixelFormat(format), &props);

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
