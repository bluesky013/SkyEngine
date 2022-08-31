//
// Created by Zach Lee on 2022/8/23.
//

#include <vulkan/Device.h>
#include <vulkan/QueryPool.h>

namespace sky::drv {

    QueryPool::QueryPool(Device &dev) : DevObject(dev)
    {
    }

    QueryPool::~QueryPool()
    {
        if (pool != VK_NULL_HANDLE) {
            vkDestroyQueryPool(device.GetNativeHandle(), pool, VKL_ALLOC);
        }
    }

    bool QueryPool::Init(const Descriptor &desc)
    {
        VkQueryPoolCreateInfo poolInfo = {};
        poolInfo.sType                 = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        poolInfo.queryType             = desc.queryType;
        poolInfo.queryCount            = desc.queryCount;
        poolInfo.pipelineStatistics    = desc.pipelineStatistics;
        results.resize(desc.queryCount);

        if (poolInfo.queryType == VK_QUERY_TYPE_PIPELINE_STATISTICS) {
            queryCount           = 1;
            pipelineStaticsFlags = desc.pipelineStatistics;
        }

        if (vkCreateQueryPool(device.GetNativeHandle(), &poolInfo, VKL_ALLOC, &pool) != VK_SUCCESS) {
            return false;
        }
        return true;
    }

    VkQueryPool QueryPool::GetNativeHandle() const
    {
        return pool;
    }

    void QueryPool::ReadResults(uint32_t count, uint32_t first)
    {
        auto result = vkGetQueryPoolResults(device.GetNativeHandle(), pool, first, count, static_cast<uint32_t>(results.size()) * sizeof(uint64_t), results.data(),
                              sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);
    }

    void QueryPool::Reset()
    {
        vkResetQueryPool(device.GetNativeHandle(), pool, 0, queryCount);
    }

    const std::vector<uint64_t> &QueryPool::GetData() const
    {
        return results;
    }

    VkQueryPipelineStatisticFlags QueryPool::GetFlags() const
    {
        return pipelineStaticsFlags;
    }

} // namespace sky::drv