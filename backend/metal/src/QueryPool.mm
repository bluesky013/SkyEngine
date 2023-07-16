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

} // namespace sky::mtl

