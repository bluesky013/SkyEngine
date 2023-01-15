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
    };
    using PipelineLayoutPtr = std::shared_ptr<PipelineLayout>;
}