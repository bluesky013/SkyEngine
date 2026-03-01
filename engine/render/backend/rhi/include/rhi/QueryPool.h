//
// Created by Zach Lee on 2023/7/15.
//

#pragma once

#include <memory>
#include <rhi/Core.h>
#include <rhi/Buffer.h>

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
        virtual uint32_t GetStride() const { return 0; }
        virtual void ConvertPipelineStatisticData(const BufferPtr &buffer, uint32_t offset, uint32_t size, PipelineStatisticData &result) const {};

        QueryType GetQueryType() const { return descriptor.type; }
        uint32_t GetQueryCount() const { return descriptor.queryCount; }

    protected:
        Descriptor descriptor;
    };
    using QueryPoolPtr = std::shared_ptr<QueryPool>;

} // namespace sky::rhi
