//
// Created by Zach Lee on 2022/1/26.
//

#pragma once

#include <vulkan/DevObject.h>
#include <vulkan/DescriptorSetLayout.h>
#include <vulkan/DescriptorSetPool.h>
#include <memory>

namespace sky::drv {
    class DescriptorSet : public DevObject {
    public:
        DescriptorSet(Device& device) : DevObject(device) {}
        ~DescriptorSet();

        static std::shared_ptr<DescriptorSet> Allocate(DescriptorSetPoolPtr pool, DescriptorSetLayoutPtr layout);

    private:
        friend class DescriptorSetPool;
        DescriptorSetLayoutPtr layout;
        DescriptorSetPoolPtr pool;
        VkDescriptorSet handle = VK_NULL_HANDLE;
    };

    using DescriptorSetPtr = std::shared_ptr<DescriptorSet>;

}