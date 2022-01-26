//
// Created by Zach Lee on 2022/1/26.
//

#pragma once

#include <vulkan/DevObject.h>
#include <vulkan/DescriptorSetLayout.h>
#include <memory>

namespace sky::drv {
    class DescriptorSetPool;

    class DescriptorSet : public DevObject {
    public:
        DescriptorSet(Device& device) : DevObject(device) {}
        ~DescriptorSet();

    private:
        friend class DescriptorSetPool;
        DescriptorSetLayoutPtr layout;
        std::weak_ptr<DescriptorSetPool> pool;
        VkDescriptorSet handle = VK_NULL_HANDLE;
    };

    using DescriptorSetPtr = std::shared_ptr<DescriptorSet>;

}