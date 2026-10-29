//
// Created by blues on 2026/3/29.
//

#include <VulkanDevice.h>
#include <common/FrameTransientPool.h>

#include "VulkanDeviceCapabilities.h"
#include "rdg/VulkanRenderPassCache.h"
#include <VulkanCommandPool.h>
#include <VulkanConversion.h>
#include <VulkanFence.h>
#include <VulkanInstance.h>
#include <VulkanPipelineState.h>
#include <VulkanResourceGroup.h>
#include <VulkanSemaphore.h>
#include <VulkanSwapChain.h>
#include <core/logger/Logger.h>
#include <cstring>
#include <rdg/VulkanDeviceFrameContext.h>
#include <vector>

static const char *TAG                          = "AuroraVulkan";
static const char *PORTABILITY_SUBSET_EXTENSION = "VK_KHR_portability_subset";

namespace sky::aurora {

    VulkanDevice::VulkanDevice(VulkanInstance &inst)
        : transientPool(std::make_unique<FrameTransientPool>(*this)), renderPassCache(std::make_unique<VulkanRenderPassCache>(*this)), instance(inst)
    {
    }

    VulkanDevice::~VulkanDevice()
    {
        WaitIdle();
        pipelineCache.Clear();
        renderPassCache.reset();
        transientPool.reset();
        for (auto &q : queues) {
            q.reset();
        }
        if (allocator != VK_NULL_HANDLE) {
            vmaDestroyAllocator(allocator);
            allocator = VK_NULL_HANDLE;
        }
        if (device != VK_NULL_HANDLE) {
            deviceFn.vkDestroyDevice(device, nullptr);
        }
    }

    bool VulkanDevice::OnInit(const DeviceInit &init)
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
        const auto apiVersion = gpuProperties.properties.apiVersion;
        LOG_I(TAG, "physical device Vulkan API: %u.%u.%u", VK_API_VERSION_MAJOR(apiVersion), VK_API_VERSION_MINOR(apiVersion),
              VK_API_VERSION_PATCH(apiVersion));

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
        capability.maxThreads       = std::max(std::thread::hardware_concurrency(), 1U);
        capability.anisotropyEnable = gpuFeatures.samplerAnisotropy == VK_TRUE;

        capability.minUniformBufferOffsetAlignment = gpuProperties.properties.limits.minUniformBufferOffsetAlignment;
        capability.minStorageBufferOffsetAlignment = gpuProperties.properties.limits.minStorageBufferOffsetAlignment;

        // Vulkan NDC has +Y pointing down.
        capability.clipSpaceYDown = true;

        capability.isUMA = false;
        for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i) {
            const auto flags = memoryProperties.memoryTypes[i].propertyFlags;
            if ((flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) && (flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) &&
                (flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
                capability.isUMA = true;
                break;
            }
        }

        // tier2 bindless descriptor heap (VK_EXT_descriptor_heap) is not
        // implemented on Vulkan yet; keep the feature off explicitly so callers
        // can gate on GetFeature() instead of getting a silent nullptr.
        feature.descriptorHeap = false;

        LOG_I(TAG, "sampler anisotropy: %s", capability.anisotropyEnable ? "enabled" : "disabled");
        LOG_I(TAG, "isUMA: %s", capability.isUMA ? "yes" : "no");
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
        VulkanDeviceNegotiation negotiated;
        if (const auto *error = NegotiateVulkanDevice(instFn.vkGetPhysicalDeviceFeatures2, gpu, gpuProperties.properties.apiVersion,
                                                      supportedExtensions, negotiated)) {
            LOG_E(TAG, "%s", error);
            return false;
        }
        gpuFeatures       = negotiated.core.features;
        dynamicRendering  = negotiated.dynamicRendering;
        renderPass2       = negotiated.renderPass2;
        enabledExtensions = negotiated.extensions;

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
            if ((props.queueFlags & VK_QUEUE_COMPUTE_BIT) != 0 && (props.queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0 &&
                computeQueueFamily == UINT32_MAX) {
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
        float                                queuePriority = 1.0f;

        auto addQueue = [&](uint32_t family) {
            for (const auto &info : queueCreateInfos) {
                if (info.queueFamilyIndex == family) {
                    return;
                }
            }
            VkDeviceQueueCreateInfo queueInfo = {};
            queueInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueInfo.queueFamilyIndex        = family;
            queueInfo.queueCount              = 1;
            queueInfo.pQueuePriorities        = &queuePriority;
            queueCreateInfos.push_back(queueInfo);
        };

        addQueue(graphicsQueueFamily);
        addQueue(computeQueueFamily);
        addQueue(transferQueueFamily);

        // device extensions
        enabledExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
        if (HasVulkanDeviceExtension(supportedExtensions, PORTABILITY_SUBSET_EXTENSION)) {
            enabledExtensions.push_back(PORTABILITY_SUBSET_EXTENSION);
        }

        VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT maintenance{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_EXT};
        if (instance.HasSurfaceMaintenance() && HasVulkanDeviceExtension(supportedExtensions, VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME)) {
            VkPhysicalDeviceFeatures2 queried{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
            queried.pNext = &maintenance;
            instFn.vkGetPhysicalDeviceFeatures2(gpu, &queried);
            swapchainMaintenance = maintenance.swapchainMaintenance1 == VK_TRUE;
            if (swapchainMaintenance)
                enabledExtensions.push_back(VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME);
        }
        for (const char *extension : enabledExtensions)
            LOG_I(TAG, "enabled device extension: %s", extension);
        LOG_I(TAG, "enabled rendering paths: dynamic=%u render-pass2=%u", dynamicRendering, renderPass2);

        VkPhysicalDeviceFeatures enabledCoreFeatures{};
        enabledCoreFeatures.samplerAnisotropy = gpuFeatures.samplerAnisotropy;
        if (swapchainMaintenance) {
            if (dynamicRendering)
                negotiated.rendering.pNext = &maintenance;
            else
                negotiated.synchronization.pNext = &maintenance;
        }

        VkDeviceCreateInfo createInfo      = {};
        createInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.pNext                   = &negotiated.draw;
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
        LoadDeviceFunctions(instFn.vkGetDeviceProcAddr, device, deviceFn, gpuProperties.properties.apiVersion, dynamicRendering, renderPass2);
        if (!HasVulkanRequiredFunctions(deviceFn, dynamicRendering, renderPass2)) {
            LOG_E(TAG, "enabled Vulkan features have missing device commands");
            return false;
        }

        // retrieve queues
        VkQueue graphicsQueue = VK_NULL_HANDLE;
        VkQueue computeQueue  = VK_NULL_HANDLE;
        VkQueue transferQueue = VK_NULL_HANDLE;
        deviceFn.vkGetDeviceQueue(device, graphicsQueueFamily, 0, &graphicsQueue);
        deviceFn.vkGetDeviceQueue(device, computeQueueFamily, 0, &computeQueue);
        deviceFn.vkGetDeviceQueue(device, transferQueueFamily, 0, &transferQueue);

        queues[static_cast<size_t>(QueueType::GRAPHICS)] =
            std::make_unique<VulkanQueue>(*this, QueueType::GRAPHICS, graphicsQueue, graphicsQueueFamily);
        queues[static_cast<size_t>(QueueType::COMPUTE)] = std::make_unique<VulkanQueue>(*this, QueueType::COMPUTE, computeQueue, computeQueueFamily);
        queues[static_cast<size_t>(QueueType::TRANSFER)] =
            std::make_unique<VulkanQueue>(*this, QueueType::TRANSFER, transferQueue, transferQueueFamily);

        return true;
    }

    bool VulkanDevice::CreateAllocator()
    {
        VmaVulkanFunctions vulkanFunctions    = {};
        vulkanFunctions.vkGetInstanceProcAddr = instance.GetGlobalFn().vkGetInstanceProcAddr;
        vulkanFunctions.vkGetDeviceProcAddr   = instance.GetInstanceFn().vkGetDeviceProcAddr;

        VmaAllocatorCreateInfo allocatorCI = {};
        allocatorCI.physicalDevice         = gpu;
        allocatorCI.device                 = device;
        allocatorCI.instance               = instance.GetNativeHandle();
        allocatorCI.pVulkanFunctions       = &vulkanFunctions;
        allocatorCI.vulkanApiVersion       = gpuProperties.properties.apiVersion;
#if SKY_PLATFORM_ANDROID
        // Android's VMA build supports API versions through 1.3.
        allocatorCI.vulkanApiVersion = std::min(allocatorCI.vulkanApiVersion, VK_API_VERSION_1_3);
#endif

        const VkResult res = vmaCreateAllocator(&allocatorCI, &allocator);
        if (res != VK_SUCCESS) {
            LOG_E(TAG, "vmaCreateAllocator failed: %d", res);
            return false;
        }
        return true;
    }

    uint32_t VulkanDevice::GetQueueFamilyIndex(QueueType type) const
    {
        switch (type) {
        case QueueType::COMPUTE: return computeQueueFamily;
        case QueueType::TRANSFER: return transferQueueFamily;
        default: return graphicsQueueFamily;
        }
    }

    Queue *VulkanDevice::GetQueue(QueueType type)
    {
        return queues[static_cast<size_t>(type)].get();
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

    DeviceFrameContext *VulkanDevice::CreateFrameContext(const DeviceFrameContextInitInfo &info)
    {
        return BackendFrameContext::Create<VulkanDeviceFrameContext>(this, info);
    }

    DirectFrameContext *VulkanDevice::CreateDirectFrameContext(const DeviceFrameContextInitInfo &info)
    {
        return BackendFrameContext::CreateDirect<VulkanDeviceFrameContext>(this, info);
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

    ResourceGroup *VulkanDevice::CreateResourceGroup(const ResourceGroup::Descriptor &desc)
    {
        auto *group = new VulkanResourceGroup(*this);
        if (!group->Init(desc)) {
            delete group;
            return nullptr;
        }
        return group;
    }

    DescriptorHeap *VulkanDevice::CreateDescriptorHeap(const DescriptorHeap::Descriptor &desc)
    {
        (void)desc;
        return nullptr; // TODO: VK_EXT_descriptor_heap (aurora-resource-group tier2)
    }

    SwapChain *VulkanDevice::CreateSwapChain(const SwapChain::Descriptor &desc)
    {
        auto *sc = new VulkanSwapChain(*this);
        if (!sc->Init(desc)) {
            delete sc;
            return nullptr;
        }
        return sc;
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
        PixelFormatFeatureFlags    result;

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

    VulkanContext::VulkanContext(VulkanDevice &dev, QueueType type, VkCommandBufferLevel level) : device(dev)
    {
        pool = std::make_unique<VulkanCommandPool>(device, device.GetQueueFamilyIndex(type), level);
        pool->Init();
    }

    void VulkanContext::OnAttach(uint32_t /*threadIndex*/)
    {
    }

    void VulkanContext::OnDetach()
    {
        if (pool) {
            pool->Reset();
        }
    }

} // namespace sky::aurora
