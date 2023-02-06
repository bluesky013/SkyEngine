//
// Created by Zach Lee on 2022/11/10.
//

#pragma once

#include <rhi/Core.h>

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
        };

        struct Descriptor {
            std::vector<SetBinding> bindings;
        };
    };
    using DescriptorSetLayoutPtr = std::shared_ptr<DescriptorSetLayout>;
}