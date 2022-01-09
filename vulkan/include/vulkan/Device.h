//
// Created by Zach Lee on 2021/11/7.
//

#pragma once
#include <vulkan/vulkan.h>
#include <vulkan/Queue.h>
#include <vulkan/CacheManager.h>
#include <vk_mem_alloc.h>
#include <core/template/ReferenceObject.h>
#include <vector>

namespace sky::drv {

    class Driver;

    class Device {
    public:
        ~Device();

        struct Descriptor {
        };

        template <typename T>
        inline std::shared_ptr<T> CreateDeviceObject(const typename T::Descriptor& des)
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

        struct QueueFilter {
            VkQueueFlags preferred = 0;
        };

        Queue* GetQueue(const QueueFilter&) const;

        VkSampler GetSampler(uint32_t hash, VkSamplerCreateInfo* samplerInfo = nullptr);

        VkPipelineLayout GetPipelineLayout(uint32_t hash, VkPipelineLayoutCreateInfo* = nullptr);

        VkDescriptorSetLayout GetDescriptorSetLayout(uint32_t hash, VkDescriptorSetLayoutCreateInfo* = nullptr);

    private:
        bool Init(const Descriptor&, bool enableDebug);

        friend class Driver;
        Device(Driver&);
        Driver& driver;
        VkPhysicalDevice phyDev;
        VkDevice device;
        VmaAllocator allocator;

        VkPhysicalDeviceProperties phyProps;
        VkPhysicalDeviceFeatures phyFeatures;

        std::vector<VkQueueFamilyProperties> queueFamilies;
        std::vector<QueuePtr> queues;

        CacheManager<VkSampler> samplers;
        CacheManager<VkDescriptorSetLayout> setLayouts;
        CacheManager<VkPipelineLayout> pipelineLayouts;
        CacheManager<VkPipeline> pipelines;
    };

}
