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
    "VK_KHR_swapchain",
#ifdef __APPLE__
    "VK_KHR_portability_subset"
#endif
};

const std::vector<const char*> VALIDATION_LAYERS = {
    "VK_LAYER_KHRONOS_validation"
};

namespace sky::drv {

    Device::Device(Driver& drv) : driver(drv), phyDev(VK_NULL_HANDLE), device(VK_NULL_HANDLE), allocator(VK_NULL_HANDLE)
    {
        samplers.SetUp([this](VkSampler sampler) {
            vkDestroySampler(device, sampler, VKL_ALLOC);
        });

        setLayouts.SetUp([this](VkDescriptorSetLayout layout) {
            vkDestroyDescriptorSetLayout(device, layout, VKL_ALLOC);
        });

        pipelineLayouts.SetUp([this](VkPipelineLayout layout) {
            vkDestroyPipelineLayout(device, layout, VKL_ALLOC);
        });

        renderPasses.SetUp([this](VkRenderPass pass) {
            vkDestroyRenderPass(device, pass, VKL_ALLOC);
        });

        pipelines.SetUp([this](VkPipeline pipeline) {
            vkDestroyPipeline(device, pipeline, VKL_ALLOC);
        });

    }

    Device::~Device()
    {
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

        queues.resize(count);
        for (i = 0; i < count; ++i) {
            VkQueue queue = VK_NULL_HANDLE;
            vkGetDeviceQueue(device, i, 0, &queue);
            queues[i] = std::unique_ptr<Queue>(new Queue(*this, queue, i));
            queues[i]->Setup();
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
        return driver.GetInstance();
    }

    Queue* Device::GetQueue(const QueueFilter& filter) const
    {
        Queue* res = nullptr;
        for (uint32_t i = 0; i < queueFamilies.size(); ++i) {
            if ((queueFamilies[i].queueFlags & filter.preferred) == filter.preferred) {
                res = queues[i].get();
            }

            if (queueFamilies[i].queueFlags == filter.preferred) {
                return queues[i].get();
            }
        }
        return res;
    }

    VkSampler Device::GetSampler(uint32_t hash, VkSamplerCreateInfo* samplerInfo)
    {
        if (samplerInfo == nullptr) {
            return samplers.Find(hash);
        }

        return samplers.FindOrEmplace(hash, [this, samplerInfo]() {
            VkSampler sampler = VK_NULL_HANDLE;
            auto rst = vkCreateSampler(device, samplerInfo, VKL_ALLOC, &sampler);
            if (rst != VK_SUCCESS) {
                LOG_E(TAG, "create Sampler failed, %d", rst);
            }
            return sampler;
        });
    }

    VkPipelineLayout Device::GetPipelineLayout(uint32_t hash, VkPipelineLayoutCreateInfo* layoutInfo)
    {
        if (layoutInfo == nullptr) {
            return pipelineLayouts.Find(hash);
        }

        return pipelineLayouts.FindOrEmplace(hash, [this, layoutInfo]() {
            VkPipelineLayout layout = VK_NULL_HANDLE;
            auto rst = vkCreatePipelineLayout(device, layoutInfo, VKL_ALLOC, &layout);
            if (rst != VK_SUCCESS) {
                LOG_E(TAG, "create PipelineLayout failed, %d", rst);
            }
            return layout;
        });
    }

    VkDescriptorSetLayout Device::GetDescriptorSetLayout(uint32_t hash, VkDescriptorSetLayoutCreateInfo* layoutInfo)
    {
        if (layoutInfo == nullptr) {
            return setLayouts.Find(hash);
        }

        return setLayouts.FindOrEmplace(hash, [this, layoutInfo]() {
            VkDescriptorSetLayout layout = VK_NULL_HANDLE;
            auto rst = vkCreateDescriptorSetLayout(device, layoutInfo, VKL_ALLOC, &layout);
            if (rst != VK_SUCCESS) {
                LOG_E(TAG, "create DescriptorSetLayout failed, %d", rst);
            }
            return layout;
        });
    }

    VkRenderPass Device::GetRenderPass(uint32_t hash, VkRenderPassCreateInfo* passInfo)
    {
        if (passInfo == nullptr) {
            return renderPasses.Find(hash);
        }

        return renderPasses.FindOrEmplace(hash, [this, passInfo]() {
            VkRenderPass pass = VK_NULL_HANDLE;
            auto rst = vkCreateRenderPass(device, passInfo, VKL_ALLOC, &pass);
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
            auto rst = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, pipelineInfo, VKL_ALLOC, &pipeline);
            if (rst != VK_SUCCESS) {
                LOG_E(TAG, "create Pipeline failed, %d", rst);
            }
            return pipeline;
        });
    }

}

