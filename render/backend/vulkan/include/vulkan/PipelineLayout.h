//
// Created by Zach Lee on 2022/1/9.
//

#pragma once
#include "rhi/PipelineLayout.h"
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

    class PipelineLayout : public rhi::PipelineLayout, public DevObject {
    public:
        ~PipelineLayout() override = default;

        DescriptorSetPtr Allocate(DescriptorSetPoolPtr pool, uint32_t slot);

        uint32_t GetHash() const { return hash; }
        uint32_t GetSetNumber() const { return static_cast<uint32_t>(desLayouts.size()); }
        uint32_t GetDynamicNum() const { return dynamicNum; }
        VkPipelineLayout GetNativeHandle() const { return layout; }

        DescriptorSetLayoutPtr GetLayout(uint32_t slot) const;
        rhi::DescriptorSetLayoutPtr GetSetLayout(uint32_t set) const override;
        const std::vector<DescriptorSetLayoutPtr> &GetLayouts() const { return desLayouts; }
        const std::vector<VkPushConstantRange> &GetConstantRanges() const { return pushConstants; }
    private:
        friend class Device;
        explicit PipelineLayout(Device &);

        bool Init(const Descriptor &);
        VkPipelineLayout                    layout;
        uint32_t                            dynamicNum;
        uint32_t                            hash;
        std::vector<VkPushConstantRange>    pushConstants;
        std::vector<DescriptorSetLayoutPtr> desLayouts;
    };

    using PipelineLayoutPtr = std::shared_ptr<PipelineLayout>;

} // namespace sky::vk
