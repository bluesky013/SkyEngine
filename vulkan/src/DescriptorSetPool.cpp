//
// Created by Zach Lee on 2022/1/26.
//

#include <vulkan/DescriptorSet.h>
#include <vulkan/DescriptorSetPool.h>
#include <vulkan/Device.h>

namespace sky::drv {

    DescriptorSetPool::DescriptorSetPool(Device &dev) : DevObject(dev)
    {
    }

    DescriptorSetPool::~DescriptorSetPool()
    {
        if (pool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(device.GetNativeHandle(), pool, VKL_ALLOC);
        }
    }

    bool DescriptorSetPool::Init(const Descriptor &desc)
    {
        VkDescriptorPoolCreateInfo poolInfo = {};
        poolInfo.sType                      = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.pNext                      = nullptr;
        poolInfo.flags                      = 0;
        poolInfo.maxSets                    = desc.maxSets;
        poolInfo.poolSizeCount              = desc.num;
        poolInfo.pPoolSizes                 = desc.sizes;
        auto result                         = vkCreateDescriptorPool(device.GetNativeHandle(), &poolInfo, VKL_ALLOC, &pool);
        if (result != VK_SUCCESS) {
            return false;
        }
        return true;
    }

    void DescriptorSetPool::Free(DescriptorSet &set)
    {
        if (set.handle != VK_NULL_HANDLE) {
            auto  layout = set.layout;
            auto  hash   = layout->GetHash();
            auto &list   = freeList[hash];
            list.emplace_back(set.handle);
            set.handle = VK_NULL_HANDLE;
        }
    }
} // namespace sky::drv