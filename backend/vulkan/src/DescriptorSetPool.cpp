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
        if (pool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(device.GetNativeHandle(), pool, VKL_ALLOC);
        }
    }

    bool DescriptorSetPool::Init(const Descriptor &desc)
    {
        VkDescriptor vkDesc = {};
        std::vector<VkDescriptorPoolSize> sizes;
        for (auto &[type, size] : desc.sizes) {
            sizes.emplace_back(VkDescriptorPoolSize{FromRHI(type), size});
        }
        vkDesc.maxSets = desc.maxSets;
        vkDesc.num = static_cast<uint32_t>(sizes.size());
        vkDesc.sizes = sizes.data();
        return Init(vkDesc);
    }

    bool DescriptorSetPool::Init(const VkDescriptor &desc)
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

    rhi::DescriptorSetPtr DescriptorSetPool::Allocate(const rhi::DescriptorSet::Descriptor &desc)
    {
        return DescriptorSet::Allocate(shared_from_this(), std::static_pointer_cast<DescriptorSetLayout>(desc.layout));
    }
} // namespace sky::vk
