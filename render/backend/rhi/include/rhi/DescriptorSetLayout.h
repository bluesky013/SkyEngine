//
// Created by Zach Lee on 2022/11/10.
//

#pragma once

#include <rhi/Core.h>
#include <string>
#include <vector>
#include <unordered_map>

namespace sky::rhi {

    class DescriptorSetLayout {
    public:
        DescriptorSetLayout()          = default;
        virtual ~DescriptorSetLayout() = default;

        struct SetBinding {
            DescriptorType   type    = DescriptorType::SAMPLER;
            uint32_t         count   = 1;
            uint32_t         binding = 0;
            ShaderStageFlags visibility;
            std::string      name;
        };

        struct Descriptor {
            std::vector<SetBinding> bindings;
        };

        uint32_t GetHash() const { return hash; }
        uint32_t GetDynamicNum() const { return dynamicNum; }
        uint32_t GetDescriptorNum() const { return descriptorNum; }
        uint32_t GetDescriptorCount() const { return descriptorCount; }
        uint32_t GetDescriptorSetOffsetByBinding(uint32_t binding) const { return bindingIndices.at(binding).first; }
        uint32_t GetDescriptorSetCountByBinding(uint32_t binding) const { return bindingIndices.at(binding).second; }
        const std::vector<SetBinding> &GetBindings() const { return bindings; }

    protected:
        uint32_t descriptorCount = 0;
        uint32_t hash = 0;
        uint32_t dynamicNum = 0;
        uint32_t descriptorNum = 0;
        std::vector<SetBinding> bindings;
        std::unordered_map<uint32_t, std::pair<uint32_t, uint32_t>> bindingIndices; // binding -> [offset, count]
    };
    using DescriptorSetLayoutPtr = std::shared_ptr<DescriptorSetLayout>;
}
