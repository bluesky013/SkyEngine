//
// Created by Zach Lee on 2022/1/9.
//

#pragma once
#include "vulkan/DevObject.h"
#include "vulkan/vulkan.h"
#include "vulkan/Sampler.h"
#include <map>

namespace sky::drv {

    class Device;

    class DescriptorSetLayout : public DevObject {
    public:
        ~DescriptorSetLayout() = default;

        struct SetBinding {
            VkDescriptorType   descriptorType   = VK_DESCRIPTOR_TYPE_SAMPLER;
            uint32_t           descriptorCount  = 0;
            VkShaderStageFlags stageFlags       = 0;
            uint32_t           immutableSampler = 0;
        };

        struct Descriptor {
            std::map<uint32_t, SetBinding> bindings;
        };

        bool Init(const Descriptor&);

        VkDescriptorSetLayout GetNativeHandle() const;

        uint32_t GetHash() const;

    private:
        friend class Device;
        DescriptorSetLayout(Device&);

        VkDescriptorSetLayout layout;
        uint32_t hash;
    };

    using DescriptorSetLayoutPtr = std::shared_ptr<DescriptorSetLayout>;

}