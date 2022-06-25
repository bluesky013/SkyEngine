//
// Created by Zach Lee on 2022/1/26.
//

#include <vulkan/DescriptorSet.h>
#include <vulkan/DescriptorSetPool.h>
#include <vulkan/Device.h>

namespace sky::drv {

    DescriptorSet::~DescriptorSet()
    {
        if (pool) {
            pool->Free(*this);
        }
    }

    VkDescriptorSet DescriptorSet::GetNativeHandle() const
    {
        return handle;
    }

    DescriptorSetPtr DescriptorSet::Allocate(DescriptorSetPoolPtr pool, DescriptorSetLayoutPtr layout)
    {
        auto setCreateFn = [&pool, &layout](VkDescriptorSet set) {
            auto setPtr = std::make_shared<DescriptorSet>(pool->device);
            setPtr->handle = set;
            setPtr->layout = layout;
            setPtr->pool = pool;
            return setPtr;
        };

        auto hash = layout->GetHash();
        auto iter = pool->freeList.find(hash);
        if (iter != pool->freeList.end() && !iter->second.empty()) {
            auto back = iter->second.back();
            iter->second.pop_back();
            return setCreateFn(back);
        }

        auto vl = layout->GetNativeHandle();
        VkDescriptorSetAllocateInfo setInfo = {};
        setInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        setInfo.pNext = nullptr;
        setInfo.descriptorPool = pool->pool;
        setInfo.descriptorSetCount = 1;
        setInfo.pSetLayouts = &vl;

        VkDescriptorSet set = VK_NULL_HANDLE;
        auto result = vkAllocateDescriptorSets(pool->device.GetNativeHandle(), &setInfo, &set);
        if (result != VK_SUCCESS) {
            return {};
        }

        return setCreateFn(set);
    }

}