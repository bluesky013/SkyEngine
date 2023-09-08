//
// Created by Zach Lee on 2022/11/10.
//

#pragma once

#include <rhi/Core.h>
#include <rhi/DescriptorSetLayout.h>

namespace sky::rhi {

    class PipelineLayout {
    public:
        PipelineLayout() = default;
        virtual ~PipelineLayout() = default;

        struct Descriptor {
            std::vector<DescriptorSetLayoutPtr> layouts;
        };

        virtual DescriptorSetLayoutPtr GetSetLayout(uint32_t set) const = 0;
    };
    using PipelineLayoutPtr = std::shared_ptr<PipelineLayout>;
}
