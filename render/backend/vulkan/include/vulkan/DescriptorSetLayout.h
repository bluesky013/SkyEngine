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

        struct SetBinding {
            VkDescriptorType         descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLER;
            uint32_t                 descriptorCount = 1;
            VkShaderStageFlags       stageFlags      = 0;
            uint32_t                 size            = 0; // TODO remove later
            VkDescriptorBindingFlags bindingFlags    = 0;
        };

        struct VkDescriptor {
            std::map<uint32_t, SetBinding> bindings;
        };

        bool Init(const Descriptor &);
        bool Init(const VkDescriptor &);

        VkDescriptorSetLayout GetNativeHandle() const { return layout; }

        const std::map<uint32_t, SetBinding> &GetDescriptorTable() const { return info.bindings; }
        const std::vector<uint32_t> &GetVariableDescriptorCounts() const { return variableDescriptorCounts; }
        VkDescriptorUpdateTemplate GetUpdateTemplate() const { return updateTemplate; }

    private:
        friend class Device;
        explicit DescriptorSetLayout(Device &);

        VkDescriptor          info;
        VkDescriptorSetLayout layout;
        VkDescriptorUpdateTemplate updateTemplate;
        std::vector<uint32_t> variableDescriptorCounts;
    };

    using DescriptorSetLayoutPtr = std::shared_ptr<DescriptorSetLayout>;

} // namespace sky::vk
