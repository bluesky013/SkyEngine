//
// Created by Zach Lee on 2022/1/9.
//

#pragma once
#include "vulkan/DevObject.h"
#include "vulkan/vulkan.h"
#include "vulkan/DescriptorSet.h"
#include "vulkan/DescriptorSetLayout.h"
#include "vulkan/DescriptorSetPool.h"
#include <map>
#include <vector>

namespace sky::drv {

    class Device;

    class PipelineLayout : public DevObject {
    public:
        ~PipelineLayout();

        struct Descriptor {
            std::vector<DescriptorSetLayout::Descriptor> desLayouts;
            std::vector<VkPushConstantRange> pushConstants;
        };

        bool Init(const Descriptor&);

        VkPipelineLayout GetNativeHandle() const;

        uint32_t GetHash() const;

        DescriptorSetPtr Allocate(DescriptorSetPool& pool, uint32_t slot);

    private:
        friend class Device;
        PipelineLayout(Device&);

        VkPipelineLayout layout;
        uint32_t hash;
        std::vector<DescriptorSetLayoutPtr> desLayouts;
    };

    using PipelineLayoutPtr = std::shared_ptr<PipelineLayout>;

}