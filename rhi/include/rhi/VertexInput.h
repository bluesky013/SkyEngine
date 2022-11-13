//
// Created by Zach Lee on 2022/11/13.
//

#pragma once

#include <rhi/Core.h>

namespace sky::rhi {

    class VertexInput {
    public:
        VertexInput() = default;
        virtual ~VertexInput() = default;

        struct Descriptor {
        };
    };
    using VertexInputPtr = std::shared_ptr<VertexInput>;
}