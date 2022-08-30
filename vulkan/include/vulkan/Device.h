//
// Created by Zach Lee on 2021/11/7.
//

#pragma once
#include <core/template/ReferenceObject.h>
#include <vector>
#include <vk_mem_alloc.h>
#include <vulkan/CacheManager.h>
#include <vulkan/Queue.h>
#include <vulkan/vulkan.h>

namespace sky::drv {

    class Driver;

    class Device {
    public:
        ~Device();

        struct Descriptor {};

        template <typename T>
        inline std::shared_ptr<T> CreateDeviceObject(const typename T::Descriptor &des)
        {
            auto res = new T(*this);
            if (!res->Init(des)) {
                delete res;
                res = nullptr;
            }
            return std::shared_ptr<T>(res);
        }

        VmaAllocator GetAllocator() const;

        VkDevice GetNativeHandle() const;

        VkPhysicalDevice GetGpuHandle() const;

        VkInstance GetInstance() const;

        Queue *GetQueue(VkQueueFlags preferred) const;

        VkSampler GetSampler(uint32_t hash, VkSamplerCreateInfo *samplerInfo = nullptr);

        VkPipelineLayout GetPipelineLayout(uint32_t hash, VkPipelineLayoutCreateInfo * = nullptr);

        VkDescriptorSetLayout GetDescriptorSetLayout(uint32_t hash, VkDescriptorSetLayoutCreateInfo * = nullptr);

        VkRenderPass GetRenderPass(uint32_t hash, VkRenderPassCreateInfo * = nullptr);

        VkPipeline GetPipeline(uint32_t hash, VkGraphicsPipelineCreateInfo * = nullptr);

        const VkPhysicalDeviceProperties &GetProperties() const;

        void WaitIdle() const;

    private:
        bool Init(const Descriptor &, bool enableDebug);

        friend class Driver;
        Device(Driver &);
        Driver          &driver;
        VkPhysicalDevice phyDev;
        VkDevice         device;
        VmaAllocator     allocator;

        VkPhysicalDeviceProperties phyProps;
        VkPhysicalDeviceFeatures   phyFeatures;

        std::vector<VkQueueFamilyProperties> queueFamilies;
        std::vector<QueuePtr>                queues;

        CacheManager<VkSampler>             samplers;
        CacheManager<VkDescriptorSetLayout> setLayouts;
        CacheManager<VkPipelineLayout>      pipelineLayouts;
        CacheManager<VkPipeline>            pipelines;
        CacheManager<VkRenderPass>          renderPasses;
    };

} // namespace sky::drv
