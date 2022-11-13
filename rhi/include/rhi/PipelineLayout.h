//
// Created by Zach Lee on 2022/11/10.
//

#pragma once

#include <rhi/Core.h>

namespace sky::rhi {

    class PipelineLayout {
    public:
        PipelineLayout() = default;
        virtual ~PipelineLayout() = default;

        struct Descriptor {
        };
    };
    using PipelineLayoutPtr = std::shared_ptr<PipelineLayout>;
}