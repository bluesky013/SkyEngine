//
// Created by Zach Lee on 2023/7/16.
//

#pragma once

#include <rhi/QueryPool.h>
#include <mtl/DevObject.h>
#import <Metal/MTLCounters.h>

namespace sky::mtl {

    class QueryPool : public rhi::QueryPool, public DevObject {
    public:
        QueryPool(Device &dev) : DevObject(dev) {}
        ~QueryPool() override;

        id<MTLCounterSampleBuffer> GetCounterSamplerBuffer() const { return sampleBuffer; }
        uint32_t GetStride() const override;

    private:
        friend class Device;
        bool Init(const Descriptor &desc);

        id<MTLCounterSampleBuffer> sampleBuffer = nil;
        uint32_t pipelineStatisticCount = 0;
    };

} // namespace sky::mtl
