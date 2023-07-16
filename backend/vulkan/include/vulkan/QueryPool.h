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

        uint32_t GetStride() const;
        void     Reset(uint32_t first, uint32_t count) const override;

    private:
        friend class Device;
        explicit QueryPool(Device &);

        bool Init(const VkDescriptor &);
        bool Init(const Descriptor &);

        void ConvertPipelineStatisticData(const rhi::BufferPtr &buffer, uint32_t offset, uint32_t size,
                                          rhi::PipelineStatisticData &result) const override;

        VkQueryPool pool;
        uint32_t    pipelineStatisticCount = 0;
        VkQueryPipelineStatisticFlags vkFlags = 0;
    };
    using QueryPoolPtr = std::shared_ptr<QueryPool>;

} // namespace sky::vk
