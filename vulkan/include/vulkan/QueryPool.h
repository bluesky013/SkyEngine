//
// Created by Zach Lee on 2022/8/23.
//

#pragma once

#include <vector>
#include <vulkan/DevObject.h>
#include <vulkan/vulkan_core.h>

namespace sky::drv {

    class QueryPool : public DevObject {
    public:
        ~QueryPool();

        struct Descriptor {
            VkQueryType                   queryType          = VK_QUERY_TYPE_PIPELINE_STATISTICS;
            uint32_t                      queryCount         = 0;
            VkQueryPipelineStatisticFlags pipelineStatistics = 0;
        };

        bool Init(const Descriptor &);

        VkQueryPool GetNativeHandle() const;

        void ReadResults(uint32_t queryCount, uint32_t firstQuery = 0);

        const std::vector<uint64_t> &GetData() const;

        VkQueryPipelineStatisticFlags GetFlags() const;

        void Reset();

    private:
        friend class Device;
        QueryPool(Device &);

        VkQueryPool                   pool;
        uint32_t                      queryCount           = 0;
        VkQueryPipelineStatisticFlags pipelineStaticsFlags = 0;
        std::vector<uint64_t>         results;
    };
    using QueryPoolPtr = std::shared_ptr<QueryPool>;

} // namespace sky::drv