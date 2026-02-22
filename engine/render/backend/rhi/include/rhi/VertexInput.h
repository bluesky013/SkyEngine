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
            uint32_t attributesNum = 0;
            uint32_t bindingsNum = 0;
            VertexAttributeDesc* attributes = nullptr;
            VertexBindingDesc* bindings = nullptr;
        };
    };
    using VertexInputPtr = std::shared_ptr<VertexInput>;
}