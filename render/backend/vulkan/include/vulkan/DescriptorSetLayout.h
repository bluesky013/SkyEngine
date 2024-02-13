//
// Created by Zach Lee on 2022/1/9.
//

#pragma once
#include "vulkan/DevObject.h"
#include "vulkan/Sampler.h"
#include "vulkan/vulkan.h"
#include "rhi/DescriptorSetLayout.h"
#include <map>
#include <vector>

namespace sky::vk {

    class Device;

    union DescriptorWriteInfo {
        VkDescriptorImageInfo      image;
        VkDescriptorBufferInfo     buffer;
        VkBufferView               bufferView;
        VkAccelerationStructureKHR accStructure;
    };

    class DescriptorSetLayout : public rhi::DescriptorSetLayout, public DevObject {
    public:
        ~DescriptorSetLayout() override;

        bool Init(const Descriptor &);

        VkDescriptorSetLayout GetNativeHandle() const { return layout; }

        const std::vector<uint32_t> &GetVariableDescriptorCounts() const { return variableDescriptorCounts; }
        VkDescriptorUpdateTemplate GetUpdateTemplate() const { return updateTemplate; }
    private:
        friend class Device;
        explicit DescriptorSetLayout(Device &);

        VkDescriptorSetLayout layout;
        VkDescriptorUpdateTemplate updateTemplate;
        std::vector<uint32_t> variableDescriptorCounts;
    };

    using DescriptorSetLayoutPtr = std::shared_ptr<DescriptorSetLayout>;

} // namespace sky::vk
