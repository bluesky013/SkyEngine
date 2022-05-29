//
// Created by Zach Lee on 2022/1/26.
//

#pragma once

#include <vulkan/DevObject.h>
#include <vulkan/vulkan.h>
#include <list>
#include <unordered_map>

namespace sky::drv {

    class DescriptorSetPool : public DevObject {
    public:
        DescriptorSetPool(Device& dev);
        ~DescriptorSetPool();

        struct Descriptor {
            uint32_t maxSets            = 0;
            uint32_t num                = 0;
            VkDescriptorPoolSize* sizes = nullptr;
        };

        bool Init(const Descriptor& desc);

    private:
        friend class DescriptorSet;
        void Free(DescriptorSet& set);

        VkDescriptorPool pool;
        using SetList = std::list<VkDescriptorSet>;
        std::unordered_map<uint32_t, SetList> freeList;
    };
    using DescriptorSetPoolPtr = std::shared_ptr<DescriptorSetPool>;

}