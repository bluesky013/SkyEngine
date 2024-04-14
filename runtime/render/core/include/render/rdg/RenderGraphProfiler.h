//
// Created by blues on 2023/9/21.
//

#pragma once

#include <rhi/QueryPool.h>

namespace sky::rdg {

    class RenderGraphProfiler {
    public:
        RenderGraphProfiler() = default;
        ~RenderGraphProfiler() = default;

        void Init();

    private:
        rhi::QueryPoolPtr queryPool;
    };

} // namespace sky::rdg