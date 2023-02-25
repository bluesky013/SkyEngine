//
// Created by Zach Lee on 2021/11/7.
//

#include <core/logger/Logger.h>
#include <vulkan/Basic.h>
#include <vulkan/Device.h>
#include <vulkan/Instance.h>

#include <vector>

static const char              *TAG         = "Vulkan";
const std::vector<const char *> DEVICE_EXTS = {"VK_KHR_swapchain",
                                               "VK_EXT_descriptor_indexing",
#ifdef __APPLE__
                                               "VK_KHR_portability_subset"
#endif
};

const std::vector<const char *> VALIDATION_LAYERS = {"VK_LAYER_KHRONOS_validation"};

namespace sky::vk {
    int32_t Device::FindProperties( uint32_t memoryTypeBits, VkMemoryPropertyFlags requiredProperties) const
    {
        auto &properties = memoryProperties.memoryProperties;
        const uint32_t memoryCount = properties.memoryTypeCount;
        for (uint32_t i = 0; i < memoryCount; ++i) {
            const bool isRequiredMemoryType  = static_cast<bool>(memoryTypeBits & (1 << i));
            const bool hasRequiredProperties = (properties.memoryTypes[i].propertyFlags & requiredProperties) == requiredProperties;
            if (isRequiredMemoryType && hasRequiredProperties) {
                return static_cast<int32_t>(i);
            }
        }
        return -1;
    }


    Device::Device(Instance &inst) : instance(inst), phyDev(VK_NULL_HANDLE), device(VK_NULL_HANDLE), allocator(VK_NULL_HANDLE)
    {
        samplers.SetUp([this](VkSampler sampler) { vkDestroySampler(device, sampler, VKL_ALLOC); });

        setLayouts.SetUp([this](VkDescriptorSetLayout layout) { vkDestroyDescriptorSetLayout(device, layout, VKL_ALLOC); });

        pipelineLayouts.SetUp([this](VkPipelineLayout layout) { vkDestroyPipelineLayout(device, layout, VKL_ALLOC); });

        renderPasses.SetUp([this](VkRenderPass pass) { vkDestroyRenderPass(device, pass, VKL_ALLOC); });

        pipelines.SetUp([this](VkPipeline pipeline) { vkDestroyPipeline(device, pipeline, VKL_ALLOC); });
    }

    Device::~Device()
    {
        if (transferQueue) {
            transferQueue->Shutdown();
            transferQueue = nullptr;
        }

        if (allocator != VK_NULL_HANDLE) {
            vmaDestroyAllocator(allocator);
        }

        queues.clear();

        samplers.Shutdown();
        setLayouts.Shutdown();
        pipelineLayouts.Shutdown();
        pipelines.Shutdown();
        renderPasses.Shutdown();

        if (device != VK_NULL_HANDLE) {
            vkDestroyDevice(device, VKL_ALLOC);
        }
    }

    template <typename ...Args>
    VkBool32 CheckFeature(bool required, bool &out, Args ...args)
    {
        VkBool32 combined = VK_TRUE;
        ((combined &= args), ...);
        out = required & static_cast<bool>(combined);
        return out;
    }

    void Device::ValidateFeature(const DeviceFeature &feature)
    {
        enabledPhyFeatures.multiViewport            = phyFeatures.features.multiViewport;
        enabledPhyFeatures.samplerAnisotropy        = phyFeatures.features.samplerAnisotropy;
        enabledPhyFeatures.pipelineStatisticsQuery  = phyFeatures.features.pipelineStatisticsQuery;
        enabledPhyFeatures.inheritedQueries         = phyFeatures.features.inheritedQueries;
        enabledPhyFeatures.fragmentStoresAndAtomics = phyFeatures.features.fragmentStoresAndAtomics;
        enabledPhyFeatures.multiDrawIndirect        = phyFeatures.features.multiDrawIndirect;

        bool sparseBinding = CheckFeature(feature.sparseBinding, enabledFeature.sparseBinding,
            phyFeatures.features.sparseBinding,
            phyFeatures.features.sparseResidencyBuffer,
            phyFeatures.features.sparseResidencyImage2D,
            phyFeatures.features.shaderResourceResidency);
        enabledPhyFeatures.sparseBinding           = sparseBinding;
        enabledPhyFeatures.sparseResidencyBuffer   = sparseBinding;
        enabledPhyFeatures.sparseResidencyImage2D  = sparseBinding;
        enabledPhyFeatures.shaderResourceResidency = sparseBinding;

        auto indexingFeature = CheckFeature(feature.descriptorIndexing, enabledFeature.descriptorIndexing,
            phyIndexingFeatures.runtimeDescriptorArray,
            phyIndexingFeatures.descriptorBindingVariableDescriptorCount,
            phyIndexingFeatures.shaderSampledImageArrayNonUniformIndexing);
        enabledPhyIndexingFeatures.sType                                     = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
        enabledPhyIndexingFeatures.runtimeDescriptorArray                    = indexingFeature;
        enabledPhyIndexingFeatures.descriptorBindingVariableDescriptorCount  = indexingFeature;
        enabledPhyIndexingFeatures.shaderSampledImageArrayNonUniformIndexing = indexingFeature;

        enabledPhyIndexingFeatures.pNext = &sync2Feature;
    }

    void Device::SetupAsyncTransferQueue()
    {
        std::pair<VkQueueFlags, VkQueueFlags> flagPairs[] = {
            {VK_QUEUE_TRANSFER_BIT, VK_QUEUE_GRAPHICS_BIT},
            {VK_QUEUE_TRANSFER_BIT, 0},
        };
        for (auto& [preferred, excluded] : flagPairs) {
            auto *queue = GetQueue(preferred, excluded);
            if (queue != nullptr) {
                transferQueue.reset(new AsyncTransferQueue(*this, queue));
                transferQueue->Setup();
                break;
            }
        }
    }

    bool Device::Init(const Descriptor &des, bool enableDebug)
    {
        auto *vkInstance = instance.GetInstance();

        uint32_t count = 0;
        // Pick Physical Device
        vkEnumeratePhysicalDevices(vkInstance, &count, nullptr);
        if (count == 0) {
            return false;
        }

        std::vector<VkPhysicalDevice> phyDevices(count);
        vkEnumeratePhysicalDevices(vkInstance, &count, phyDevices.data());

        phyIndexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
        phyIndexingFeatures.pNext = &sync2Feature;
        sync2Feature.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
        sync2Feature.synchronization2 = VK_TRUE;

        phyFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;

        uint32_t i = 0;
        for (; i < count; ++i) {
            auto *pDev = phyDevices[i];
            vkGetPhysicalDeviceFeatures2(pDev, &phyFeatures);
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
        float                                queuePriority = 1.0f;
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        VkDeviceQueueCreateInfo              queueInfo = {};
        queueInfo.sType                                = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        for (i = 0; i < count; ++i) {
            queueInfo.queueFamilyIndex = i;
            queueInfo.queueCount       = 1;
            queueInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.emplace_back(queueInfo);
            LOG_I(TAG, "queue family index %u, count %u, flags %u", i, queueFamilies[i].queueCount, queueFamilies[i].queueFlags);
        }
        ValidateFeature(des.feature);

        VkDeviceCreateInfo devInfo = {};
        devInfo.sType              = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        devInfo.pNext              = &enabledPhyIndexingFeatures;

        devInfo.pEnabledFeatures        = &enabledPhyFeatures;
        devInfo.enabledExtensionCount   = (uint32_t)DEVICE_EXTS.size();
        devInfo.ppEnabledExtensionNames = DEVICE_EXTS.data();

        devInfo.queueCreateInfoCount = (uint32_t)queueCreateInfos.size();
        devInfo.pQueueCreateInfos    = queueCreateInfos.data();

        if (enableDebug) {
            devInfo.enabledLayerCount   = (uint32_t)VALIDATION_LAYERS.size();
            devInfo.ppEnabledLayerNames = VALIDATION_LAYERS.data();
        }

        VkResult rst = vkCreateDevice(phyDev, &devInfo, VKL_ALLOC, &device);
        if (rst != VK_SUCCESS) {
            LOG_E(TAG, "create device failed -%d", rst);
            return false;
        }

        memoryProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
        vkGetPhysicalDeviceMemoryProperties2(phyDev, &memoryProperties);

        VmaAllocatorCreateInfo allocatorInfo = {};
        allocatorInfo.device           = device;
        allocatorInfo.physicalDevice   = phyDev;
        allocatorInfo.instance         = instance.GetInstance();
        allocatorInfo.vulkanApiVersion = VK_MAKE_API_VERSION(0, 1, 3, 0);
        rst                            = vmaCreateAllocator(&allocatorInfo, &allocator);
        if (rst != VK_SUCCESS) {
            LOG_E(TAG, "create allocator failed -%d", rst);
            return false;
        }

        queues.resize(count);
        for (i = 0; i < count; ++i) {
            VkQueue queue = VK_NULL_HANDLE;
            vkGetDeviceQueue(device, i, 0, &queue);
            queues[i] = std::unique_ptr<Queue>(new Queue(*this, queue, i));
            queues[i]->Setup();
        }
        graphicsQueue = GetQueue(VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT, 0);
        SetupAsyncTransferQueue();
        return true;
    }

    VmaAllocator Device::GetAllocator() const
    {
        return allocator;
    }

    VkDevice Device::GetNativeHandle() const
    {
        return device;
    }

    VkPhysicalDevice Device::GetGpuHandle() const
    {
        return phyDev;
    }

    VkInstance Device::GetInstance() const
    {
        return instance.GetInstance();
    }

    Queue *Device::GetQueue(VkQueueFlags preferred, VkQueueFlags excluded) const
    {
        Queue *res = nullptr;
        for (uint32_t i = 0; i < queueFamilies.size(); ++i) {
            const auto& flag = queueFamilies[i].queueFlags;
            if (((flag & preferred) == preferred) && ((flag & excluded) == 0)) {
                res = queues[i].get();
            }
        }
        return res;
    }

    Queue *Device::GetGraphicsQueue() const
    {
        return graphicsQueue;
    }

    AsyncTransferQueue *Device::GetAsyncTransferQueue() const
    {
        return transferQueue.get();
    }

    VkSampler Device::GetSampler(uint32_t hash, VkSamplerCreateInfo *samplerInfo)
    {
        if (samplerInfo == nullptr) {
            return samplers.Find(hash);
        }

        return samplers.FindOrEmplace(hash, [this, samplerInfo]() {
            VkSampler sampler = VK_NULL_HANDLE;
            auto      rst     = vkCreateSampler(device, samplerInfo, VKL_ALLOC, &sampler);
            if (rst != VK_SUCCESS) {
                LOG_E(TAG, "create Sampler failed, %d", rst);
            }
            return sampler;
        });
    }

    VkPipelineLayout Device::GetPipelineLayout(uint32_t hash, VkPipelineLayoutCreateInfo *layoutInfo)
    {
        if (layoutInfo == nullptr) {
            return pipelineLayouts.Find(hash);
        }

        return pipelineLayouts.FindOrEmplace(hash, [this, layoutInfo]() {
            VkPipelineLayout layout = VK_NULL_HANDLE;
            auto             rst    = vkCreatePipelineLayout(device, layoutInfo, VKL_ALLOC, &layout);
            if (rst != VK_SUCCESS) {
                LOG_E(TAG, "create PipelineLayout failed, %d", rst);
            }
            return layout;
        });
    }

    VkDescriptorSetLayout Device::GetDescriptorSetLayout(uint32_t hash, VkDescriptorSetLayoutCreateInfo *layoutInfo)
    {
        if (layoutInfo == nullptr) {
            return setLayouts.Find(hash);
        }

        return setLayouts.FindOrEmplace(hash, [this, layoutInfo]() {
            VkDescriptorSetLayout layout = VK_NULL_HANDLE;
            auto                  rst    = vkCreateDescriptorSetLayout(device, layoutInfo, VKL_ALLOC, &layout);
            if (rst != VK_SUCCESS) {
                LOG_E(TAG, "create DescriptorSetLayout failed, %d", rst);
            }
            return layout;
        });
    }

    VkRenderPass Device::GetRenderPass(uint32_t hash, VkRenderPassCreateInfo *passInfo)
    {
        if (passInfo == nullptr) {
            return renderPasses.Find(hash);
        }

        return renderPasses.FindOrEmplace(hash, [this, passInfo]() {
            VkRenderPass pass = VK_NULL_HANDLE;
            auto         rst  = vkCreateRenderPass(device, passInfo, VKL_ALLOC, &pass);
            if (rst != VK_SUCCESS) {
                LOG_E(TAG, "create RenderPass failed, %d", rst);
            }
            return pass;
        });
    }

    VkPipeline Device::GetPipeline(uint32_t hash, VkGraphicsPipelineCreateInfo *pipelineInfo)
    {
        if (pipelineInfo == nullptr) {
            return pipelines.Find(hash);
        }

        return pipelines.FindOrEmplace(hash, [this, pipelineInfo]() {
            VkPipeline pipeline;
            auto       rst = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, pipelineInfo, VKL_ALLOC, &pipeline);
            if (rst != VK_SUCCESS) {
                LOG_E(TAG, "create Pipeline failed, %d", rst);
            }
            return pipeline;
        });
    }

    const VkPhysicalDeviceProperties &Device::GetProperties() const
    {
        return phyProps;
    }

    void Device::WaitIdle() const
    {
        vkDeviceWaitIdle(device);
    }

    bool Device::FillMemoryRequirements(VkMemoryRequirements2 &requirements, const VkMemoryDedicatedRequirements &dedicated, VkMemoryPropertyFlags flags, MemoryRequirement &out) const
    {
        int32_t index = FindProperties(requirements.memoryRequirements.memoryTypeBits, flags);
        if (index < 0) {
            return false;
        }

        out.size = requirements.memoryRequirements.size;
        out.alignment = requirements.memoryRequirements.alignment;
        out.memoryIndex = static_cast<uint32_t>(index);
        out.prefersDedicated = static_cast<bool>(dedicated.prefersDedicatedAllocation);
        out.requiresDedicated = static_cast<bool>(dedicated.requiresDedicatedAllocation);
        return true;
    }

    bool Device::GetBufferMemoryRequirements(VkBuffer buffer, VkMemoryPropertyFlags flags, MemoryRequirement &requirement) const
    {
        VkBufferMemoryRequirementsInfo2 memoryReqsInfo = {};
        memoryReqsInfo.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2;
        memoryReqsInfo.buffer = buffer;

        VkMemoryDedicatedRequirements memDedicatedReq = {};
        memDedicatedReq.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS;

        VkMemoryRequirements2 memoryReqs2 = {};
        memoryReqs2.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
        memoryReqs2.pNext = &memDedicatedReq;
        vkGetBufferMemoryRequirements2(device, &memoryReqsInfo, &memoryReqs2);

        return FillMemoryRequirements(memoryReqs2, memDedicatedReq, flags, requirement);
    }

    bool Device::GetImageMemoryRequirements(VkImage image, VkMemoryPropertyFlags flags, MemoryRequirement &requirement) const
    {
        VkImageMemoryRequirementsInfo2 memoryReqsInfo = {};
        memoryReqsInfo.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2;
        memoryReqsInfo.image = image;

        VkMemoryDedicatedRequirements memDedicatedReq = {};
        memDedicatedReq.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS;

        VkMemoryRequirements2 memoryReqs2 = {};
        memoryReqs2.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
        memoryReqs2.pNext = &memDedicatedReq;
        vkGetImageMemoryRequirements2(device, &memoryReqsInfo, &memoryReqs2);

        return FillMemoryRequirements(memoryReqs2, memDedicatedReq, flags, requirement);
    }

} // namespace sky::vk