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

    struct UpdateTemplate {
        VkDescriptorUpdateTemplate   handle;
        std::map<uint32_t, uint32_t> indices;
    };

    class DescriptorSetLayout : public rhi::DescriptorSetLayout, public DevObject {
    public:
        ~DescriptorSetLayout();

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

        VkDescriptorSetLayout GetNativeHandle() const;

        uint32_t GetHash() const;

        uint32_t GetDynamicNum() const;

        uint32_t GetDescriptorNum() const;

        const std::map<uint32_t, SetBinding> &GetDescriptorTable() const;

        const std::vector<uint32_t> &GetVariableDescriptorCounts() const;

        const UpdateTemplate &GetUpdateTemplate() const;

    private:
        friend class Device;
        DescriptorSetLayout(Device &);

        VkDescriptor          info;
        VkDescriptorSetLayout layout;
        UpdateTemplate        updateTemplate;
        uint32_t              dynamicNum;
        uint32_t              descriptorNum;
        std::vector<uint32_t> variableDescriptorCounts;
    };

    using DescriptorSetLayoutPtr = std::shared_ptr<DescriptorSetLayout>;

} // namespace sky::vk
