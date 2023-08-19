//
// Created by Zach Lee on 2023/7/16.
//

#include <mtl/QueryPool.h>
#include <core/logger/Logger.h>
#include <mtl/Device.h>

static const char* TAG = "Metal";

namespace sky::mtl {

    QueryPool::~QueryPool()
    {
        if (sampleBuffer != nil) {
            [sampleBuffer release];
            sampleBuffer = nil;
        }
    }

    uint32_t QueryPool::GetStride() const
    {
        return sizeof(MTLCounterResultStatistic) * 2;
    }

    bool QueryPool::Init(const Descriptor &desc)
    {
        descriptor = desc;

        MTLCounterSampleBufferDescriptor* bufferDesc = [[MTLCounterSampleBufferDescriptor alloc] init];
        bufferDesc.storageMode = MTLStorageModePrivate;

        if (desc.type == rhi::QueryType::PIPELINE_STATISTICS) {
            bufferDesc.counterSet = device.GetPipelineStatisticCounterSte();
            bufferDesc.sampleCount = desc.queryCount * 2;
        } if (desc.type == rhi::QueryType::TIME_STAMP) {
            bufferDesc.counterSet = device.GetTimeStampCounterSet();
            bufferDesc.sampleCount = desc.queryCount;
        }

        NSError *error = nil;
        sampleBuffer = [device.GetMetalDevice() newCounterSampleBufferWithDescriptor:bufferDesc
                                                                               error:&error];
        if (error != nil) {
            LOG_E(TAG, "create counter sample buffer failed. %s",  [[error localizedDescription] UTF8String]);
        }

        [bufferDesc release];
        return sampleBuffer != nil && error == nil;
    }

    void QueryPool::ConvertPipelineStatisticData(const rhi::BufferPtr &buffer, uint32_t offset, uint32_t size,
                                      rhi::PipelineStatisticData &result) const
    {
        MTLCounterResultStatistic mtlData[2];
        uint8_t *ptr = buffer->Map();
        memcpy(&mtlData, ptr + offset, size);
        buffer->UnMap();

        result.iaVertices      = 0;
        result.iaPrimitives    = 0;
        result.vsInvocations   = mtlData[1].vertexInvocations - mtlData[0].vertexInvocations;
        result.clipInvocations = mtlData[1].clipperInvocations - mtlData[0].clipperInvocations;
        result.clipPrimitives  = mtlData[1].clipperPrimitivesOut - mtlData[0].clipperPrimitivesOut;
        result.fsInvocations   = mtlData[1].fragmentInvocations - mtlData[0].fragmentInvocations;
        result.csInvocations   = mtlData[1].computeKernelInvocations - mtlData[0].computeKernelInvocations;
    }

} // namespace sky::mtl

