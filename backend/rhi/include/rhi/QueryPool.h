//
// Created by Zach Lee on 2023/7/15.
//

#pragma once

#include <memory>
#include <rhi/Core.h>

namespace sky::rhi {

    class QueryPool {
    public:
        QueryPool() = default;
        ~QueryPool() = default;

        struct Descriptor {
            QueryType type = QueryType::PIPELINE_STATISTICS;
            PipelineStatisticFlags pipelineStatisticFlags;
            uint32_t queryCount = 0;
        };

        virtual void Reset(uint32_t first, uint32_t count) const {}
    };
    using QueryPoolPtr = std::shared_ptr<QueryPool>;

} // namespace sky::rhi