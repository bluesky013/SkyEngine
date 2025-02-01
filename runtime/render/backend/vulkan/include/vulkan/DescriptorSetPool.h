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

        rhi::DescriptorSetPtr Allocate(const rhi::DescriptorSet::Descriptor &desc) override;

        VkDescriptorPool GetCurrentPool() const {return pools.back(); }
        VkDescriptorPool CreateNewNativePool();
    private:
        friend class Device;
        friend class DescriptorSet;
        void Free(DescriptorSet &set, VkDescriptorPool pool);
        bool Init(const Descriptor &desc);

        uint32_t maxSets = 1;
        std::vector<VkDescriptorPoolSize> sizes;
        std::vector<VkDescriptorPool>     pools;

        using SetList = std::list<std::pair<VkDescriptorSet, VkDescriptorPool>>;
        std::unordered_map<uint32_t, SetList> freeList;
    };
    using DescriptorSetPoolPtr = std::shared_ptr<DescriptorSetPool>;

} // namespace sky::vk
