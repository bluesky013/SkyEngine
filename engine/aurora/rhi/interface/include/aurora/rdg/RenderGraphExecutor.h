//
// Created by Zach Lee on 2026/7/31.
//

#pragma once

namespace sky::aurora {

    class RenderGraphExecutor {
    public:
        RenderGraphExecutor() = default;
        ~RenderGraphExecutor() = default;

        void Execute();
    };

} // namespace sky::aurora
