//
// Created by Zach Lee on 2023/4/7.
//

#pragma once

#include <rhi/CommandBuffer.h>

namespace sky::rhi {

    class PassExecutor {
    public:
        PassExecutor() = default;
        virtual ~PassExecutor() = default;

        virtual void Execute(const CommandBufferPtr &cmd) = 0;
    };

}