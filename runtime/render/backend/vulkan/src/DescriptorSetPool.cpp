//
// Created by Zach Lee on 2022/1/26.
//

#include <vulkan/DescriptorSet.h>
#include <vulkan/DescriptorSetPool.h>
#include <vulkan/Device.h>
#include <vulkan/Conversion.h>

namespace sky::vk {

    DescriptorSetPool::DescriptorSetPool(Device &dev) : DevObject(dev)
    {
    }

    DescriptorSetPool::~DescriptorSetPool()
    {
        for (auto &pool : pools) {
            vkDestroyDescriptorPool(device.GetNativeHandle(), pool, VKL_ALLOC);
        }
    }

    bool DescriptorSetPool::Init(const Descriptor &desc)
    {
        for (uint32_t i = 0; i < desc.sizeCount; ++i) {
            auto size = desc.sizeData[i];
            sizes.emplace_back(VkDescriptorPoolSize{FromRHI(size.type), size.count});
        }

        maxSets = desc.maxSets;
        CreateNewNativePool();
        return true;
    }

    VkDescriptorPool DescriptorSetPool::CreateNewNativePool()
    {
        VkDescriptorPoolCreateInfo poolInfo = {};
        poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.pNext         = nullptr;
        poolInfo.flags         = 0;
        poolInfo.maxSets       = maxSets;
        poolInfo.poolSizeCount = static_cast<uint32_t>(sizes.size());
        poolInfo.pPoolSizes    = sizes.data();

        VkDescriptorPool pool = VK_NULL_HANDLE;
        vkCreateDescriptorPool(device.GetNativeHandle(), &poolInfo, VKL_ALLOC, &pool);

        pools.emplace_back(pool);
        return pool;
    }


    void DescriptorSetPool::Free(DescriptorSet &set, VkDescriptorPool pool)
    {
        if (set.handle != VK_NULL_HANDLE) {
            auto  layout = set.layout;
            auto  hash   = layout->GetHash();
            auto &list   = freeList[hash];
            list.emplace_back(std::pair<VkDescriptorSet, VkDescriptorPool>{set.handle, pool});
            set.handle = VK_NULL_HANDLE;
        }
    }

    rhi::DescriptorSetPtr DescriptorSetPool::Allocate(const rhi::DescriptorSet::Descriptor &desc)
    {
        return DescriptorSet::Allocate(shared_from_this(), std::static_pointer_cast<DescriptorSetLayout>(desc.layout));
    }
} // namespace sky::vk
