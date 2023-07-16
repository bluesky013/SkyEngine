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
        descriptor = desc;

        VkQueryPoolCreateInfo poolInfo = {};
        poolInfo.sType                 = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        poolInfo.queryType             = FromRHI(descriptor.type);
        poolInfo.queryCount            = descriptor.queryCount;

        if (poolInfo.queryType == VK_QUERY_TYPE_PIPELINE_STATISTICS) {
            pipelineStatisticCount = device.CheckPipelineStatisticFlags(desc.pipelineStatisticFlags, descriptor.pipelineStatisticFlags);
            poolInfo.pipelineStatistics = vkFlags = FromRHI(descriptor.pipelineStatisticFlags);
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

//    void QueryPool::ReadResults(uint32_t first, uint32_t count)
//    {
//        vkGetQueryPoolResults(device.GetNativeHandle(), pool, first, count, static_cast<uint32_t>(results.size()) * sizeof(uint64_t), results.data(),
//                              sizeof(uint64_t), VK_QUERY_RESULT_64_BIT);
//    }

    uint32_t QueryPool::GetStride() const
    {
        return pipelineStatisticCount * sizeof(uint64_t);
    }

    void QueryPool::Reset(uint32_t first, uint32_t count) const
    {
        vkResetQueryPool(device.GetNativeHandle(), pool, first, count);
    }

    void QueryPool::ConvertPipelineStatisticData(const rhi::BufferPtr &buffer, uint32_t offset, uint32_t size,
                                                 rhi::PipelineStatisticData &result) const
    {
        std::vector<uint64_t> data(size / sizeof(uint64_t));
        uint8_t *ptr = buffer->Map();
        memcpy(data.data(), ptr + offset, size);
        buffer->UnMap();

        uint32_t index = 0;
        result.iaVertices      = vkFlags & VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT ? data[index++] : 0;
        result.iaPrimitives    = vkFlags & VK_QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT ? data[index++] : 0;
        result.vsInvocations   = vkFlags & VK_QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT ? data[index++] : 0;
        result.clipInvocations = vkFlags & VK_QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT ? data[index++] : 0;
        result.clipPrimitives  = vkFlags & VK_QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT ? data[index++] : 0;
        result.fsInvocations   = vkFlags & VK_QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT ? data[index++] : 0;
        result.csInvocations   = vkFlags & VK_QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT ? data[index++] : 0;
    }
} // namespace sky::vk
