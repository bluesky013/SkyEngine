//
// Created by Zach Lee on 2022/8/23.
//

#include <vulkan/Device.h>
#include <vulkan/QueryPool.h>
#include <vulkan/Conversion.h>

namespace sky::vk {

    QueryPool::QueryPool(Device &dev) : DevObject(dev)
    {
    }

    QueryPool::~QueryPool()
    {
        if (pool != VK_NULL_HANDLE) {
            vkDestroyQueryPool(device.GetNativeHandle(), pool, VKL_ALLOC);
        }
    }

    bool QueryPool::Init(const VkDescriptor &desc)
    {
        VkQueryPoolCreateInfo poolInfo = {};
        poolInfo.sType                 = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        poolInfo.queryType             = desc.queryType;
        poolInfo.queryCount            = desc.queryCount;
        results.resize(desc.queryCount);

        if (poolInfo.queryType == VK_QUERY_TYPE_PIPELINE_STATISTICS) {
            poolInfo.pipelineStatistics    = desc.pipelineStatistics;
        }

        if (vkCreateQueryPool(device.GetNativeHandle(), &poolInfo, VKL_ALLOC, &pool) != VK_SUCCESS) {
            return false;
        }
        return true;
    }

    bool QueryPool::Init(const Descriptor &desc)
    {
        VkQueryPoolCreateInfo poolInfo = {};
        poolInfo.sType                 = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        poolInfo.queryType             = FromRHI(desc.type);
        poolInfo.queryCount            = desc.queryCount;

        if (poolInfo.queryType == VK_QUERY_TYPE_PIPELINE_STATISTICS) {
            poolInfo.pipelineStatistics = FromRHI(desc.pipelineStatisticFlags);
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

    void QueryPool::ReadResults(uint32_t first, uint32_t count)
    {
        vkGetQueryPoolResults(device.GetNativeHandle(), pool, first, count, static_cast<uint32_t>(results.size()) * sizeof(uint64_t), results.data(),
                              sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);
    }

    void QueryPool::Reset(uint32_t first, uint32_t count) const
    {
        vkResetQueryPool(device.GetNativeHandle(), pool, first, count);
    }

    const std::vector<uint64_t> &QueryPool::GetData() const
    {
        return results;
    }

} // namespace sky::vk
