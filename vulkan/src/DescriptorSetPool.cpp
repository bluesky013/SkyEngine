//
// Created by Zach Lee on 2022/1/26.
//

#include <vulkan/DescriptorSetPool.h>
#include <vulkan/Device.h>

namespace sky::drv {

    DescriptorSetPool::DescriptorSetPool(Device& dev) : DevObject(dev) {}

    DescriptorSetPool::~DescriptorSetPool()
    {
        if (pool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(device.GetNativeHandle(), pool, VKL_ALLOC);
        }
    }

    bool DescriptorSetPool::Init(const Descriptor& desc)
    {
        VkDescriptorPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.pNext = nullptr;
        poolInfo.flags = 0;
        poolInfo.maxSets = desc.maxSets;
        poolInfo.poolSizeCount = desc.num;
        poolInfo.pPoolSizes = desc.sizes;
        auto result = vkCreateDescriptorPool(device.GetNativeHandle(), &poolInfo, VKL_ALLOC, &pool);
        if (result != VK_SUCCESS) {
            return false;
        }
        return true;
    }

    DescriptorSetPtr DescriptorSetPool::Allocate(DescriptorSetLayoutPtr layout)
    {
        auto setCreateFn = [layout, this](VkDescriptorSet set) {
            auto setPtr = std::make_shared<DescriptorSet>(device);
            setPtr->handle = set;
            setPtr->layout = layout;
            return setPtr;
        };

        auto hash = layout->GetHash();
        auto iter = freeList.find(hash);
        if (iter != freeList.end() && !iter->second.empty()) {
            auto back = iter->second.back();
            iter->second.pop_back();
            return setCreateFn(back);
        }

        auto vl = layout->GetNativeHandle();
        VkDescriptorSetAllocateInfo setInfo = {};
        setInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        setInfo.pNext = nullptr;
        setInfo.descriptorPool = pool;
        setInfo.descriptorSetCount = 1;
        setInfo.pSetLayouts = &vl;

        VkDescriptorSet set = VK_NULL_HANDLE;
        auto result = vkAllocateDescriptorSets(device.GetNativeHandle(), &setInfo, &set);
        if (result != VK_SUCCESS) {
            return {};
        }

        return setCreateFn(set);
    }

    void DescriptorSetPool::Free(DescriptorSet& set)
    {
        if (set.handle != VK_NULL_HANDLE) {
            auto layout = set.layout;
            auto hash = layout->GetHash();
            auto &list = freeList[hash];
            list.emplace_back(set.handle);
            set.handle = VK_NULL_HANDLE;
        }
    }
}