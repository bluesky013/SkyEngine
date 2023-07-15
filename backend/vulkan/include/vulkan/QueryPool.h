//
// Created by Zach Lee on 2022/8/23.
//

#pragma once

#include <vector>
#include <rhi/QueryPool.h>
#include <vulkan/DevObject.h>
#include <vulkan/vulkan_core.h>

namespace sky::vk {

    class QueryPool : public rhi::QueryPool, public DevObject {
    public:
        ~QueryPool() override;

        struct VkDescriptor {
            VkQueryType                   queryType          = VK_QUERY_TYPE_PIPELINE_STATISTICS;
            uint32_t                      queryCount         = 0;
            VkQueryPipelineStatisticFlags pipelineStatistics = 0;
        };

        VkQueryPool GetNativeHandle() const;

        void Reset(uint32_t first, uint32_t count) const override;

        void ReadResults(uint32_t first, uint32_t count);

        const std::vector<uint64_t> &GetData() const;

    private:
        friend class Device;
        explicit QueryPool(Device &);

        bool Init(const VkDescriptor &);
        bool Init(const Descriptor &);

        VkQueryPool           pool;
        std::vector<uint64_t> results;
    };
    using QueryPoolPtr = std::shared_ptr<QueryPool>;

} // namespace sky::vk
