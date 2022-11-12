//
// Created by Zach Lee on 2022/1/9.
//

#pragma once
#include "vulkan/DescriptorSet.h"
#include "vulkan/DescriptorSetLayout.h"
#include "vulkan/DescriptorSetPool.h"
#include "vulkan/DevObject.h"
#include "vulkan/vulkan.h"
#include <map>
#include <vector>

namespace sky::vk {

    class Device;
    class PushConstants;

    class PipelineLayout : public DevObject {
    public:
        ~PipelineLayout();

        struct VkDescriptor {
            std::vector<DescriptorSetLayout::VkDescriptor> desLayouts;
            std::vector<VkPushConstantRange>             pushConstants;
        };

        bool Init(const VkDescriptor &);

        VkPipelineLayout GetNativeHandle() const;

        uint32_t GetHash() const;

        DescriptorSetPtr Allocate(DescriptorSetPoolPtr pool, uint32_t slot);

        uint32_t GetSlotNumber() const;

        uint32_t GetDynamicNum() const;

        DescriptorSetLayoutPtr GetLayout(uint32_t slot) const;

        const std::vector<DescriptorSetLayoutPtr> &GetLayouts() const;

        const std::vector<VkPushConstantRange> &GetConstantRanges() const;

    private:
        friend class Device;
        PipelineLayout(Device &);

        VkPipelineLayout                    layout;
        uint32_t                            dynamicNum;
        uint32_t                            hash;
        std::vector<VkPushConstantRange>    pushConstants;
        std::vector<DescriptorSetLayoutPtr> desLayouts;
    };

    using PipelineLayoutPtr = std::shared_ptr<PipelineLayout>;

} // namespace sky::vk
