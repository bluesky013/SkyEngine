//
// Created by Zach Lee on 2022/1/26.
//

#pragma once

#include <list>
#include <unordered_map>
#include <rhi/DescriptorSetPool.h>
#include <vulkan/DevObject.h>
#include <vulkan/vulkan.h>

namespace sky::vk {
    class DescriptorSet;

    class DescriptorSetPool : public rhi::DescriptorSetPool, public DevObject, public std::enable_shared_from_this<DescriptorSetPool> {
    public:
        explicit DescriptorSetPool(Device &dev);
        ~DescriptorSetPool() override;

        struct VkDescriptor {
            uint32_t              maxSets = 0;
            uint32_t              num     = 0;
            VkDescriptorPoolSize *sizes   = nullptr;
        };

        rhi::DescriptorSetPtr Allocate(const rhi::DescriptorSet::Descriptor &desc) override;

    private:
        friend class Device;
        friend class DescriptorSet;
        void Free(DescriptorSet &set);
        bool Init(const Descriptor &desc);
        bool Init(const VkDescriptor &desc);

        VkDescriptorPool pool;
        using SetList = std::list<VkDescriptorSet>;
        std::unordered_map<uint32_t, SetList> freeList;
    };
    using DescriptorSetPoolPtr = std::shared_ptr<DescriptorSetPool>;

} // namespace sky::vk
