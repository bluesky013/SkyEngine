//
// Created by Zach Lee on 2022/11/13.
//

#pragma once

#include <rhi/Core.h>

namespace sky::rhi {

    class Fence {
    public:
        Fence() = default;
        virtual ~Fence() = default;

        struct Descriptor {
            bool createSignaled = true;
        };
    };

    using FencePtr = std::shared_ptr<Fence>;

}