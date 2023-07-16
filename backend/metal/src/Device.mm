//
// Created by Zach Lee on 2022/11/4.
//

#include <mtl/Device.h>
#include <mtl/Instance.h>

static const char* TAG = "Metal";

namespace sky::mtl {

    Device::Device(Instance &inst) : instance(inst)
    {
    }

    Device::~Device()
    {
        for (auto &queue : queues) {
            queue->Shutdown();
        }
        queues.clear();

        if (sharedEventListener != nullptr) {
            [sharedEventListener release];
            sharedEventListener = nil;
        }
    }

    bool Device::Init(const Descriptor &des)
    {
        const auto &mtlDevices = instance.GetMtlDevices();
        device = mtlDevices.front();

        queues.resize(2);
        for (uint32_t i = 0; i < 2; ++i) {
            queues[i] = std::unique_ptr<Queue>(new Queue(*this));
            queues[i]->queue = [device newCommandQueue];
            queues[i]->StartThread();
        }
        graphicsQueue = queues[0].get();
        transferQueue = queues[1].get();

        UpdateCounterSet();
        sharedEventListener = [[MTLSharedEventListener alloc] init];
        return true;
    }

    rhi::Queue* Device::GetQueue(rhi::QueueType type) const
    {
        return type == rhi::QueueType::GRAPHICS ? graphicsQueue : transferQueue;
    }

    void Device::UpdateCounterSet()
    {
        static constexpr rhi::PipelineStatisticFlagBits supportedFlags[] = {
            rhi::PipelineStatisticFlagBits::VS_INVOCATIONS,
            rhi::PipelineStatisticFlagBits::CLIP_INVOCATIONS,
            rhi::PipelineStatisticFlagBits::CLIP_PRIMITIVES,
            rhi::PipelineStatisticFlagBits::FS_INVOCATIONS,
            rhi::PipelineStatisticFlagBits::CS_INVOCATIONS,
        };

        pipelineStatisticCounterSet = QueryCounterSet(MTLCommonCounterSetStatistic);
        auto checkCounterFn = [this](MTLCommonCounter counter, rhi::PipelineStatisticFlagBits flagBit) {
            if (CheckCounterName(pipelineStatisticCounterSet, counter)) {
                supportedPipelineStatistics &= flagBit;
            }
        };
        checkCounterFn(MTLCommonCounterVertexInvocations, rhi::PipelineStatisticFlagBits::VS_INVOCATIONS);
        checkCounterFn(MTLCommonCounterFragmentInvocations, rhi::PipelineStatisticFlagBits::FS_INVOCATIONS);
        checkCounterFn(MTLCommonCounterComputeKernelInvocations, rhi::PipelineStatisticFlagBits::CS_INVOCATIONS);
        checkCounterFn(MTLCommonCounterClipperInvocations, rhi::PipelineStatisticFlagBits::CLIP_INVOCATIONS);
        checkCounterFn(MTLCommonCounterClipperPrimitivesOut, rhi::PipelineStatisticFlagBits::CLIP_PRIMITIVES);

        timeStampCounterSet = QueryCounterSet(MTLCommonCounterSetTimestamp);
    }

    id<MTLCounterSet> Device::QueryCounterSet(MTLCommonCounterSet counterSet) const
    {
        for (id<MTLCounterSet> cs in device.counterSets) {
            if ([cs.name isEqualToString: counterSet]) {
                return cs;
            }
        }
        return nil;
    }

    id<MTLCounter> Device::CheckCounterName(id<MTLCounterSet> set, MTLCommonCounter counterName) const
    {
        for (id<MTLCounter> counter in set.counters )
        {
            if ([counter.name isEqualToString: counterName]) {
                return counter;
            }
        }
        return nil;
    }

    uint32_t Device::CheckPipelineStatisticFlags(const rhi::PipelineStatisticFlags &val, rhi::PipelineStatisticFlags &res)
    {
        static constexpr rhi::PipelineStatisticFlagBits supportedFlags[] = {
            rhi::PipelineStatisticFlagBits::VS_INVOCATIONS,
            rhi::PipelineStatisticFlagBits::CLIP_INVOCATIONS,
            rhi::PipelineStatisticFlagBits::CLIP_PRIMITIVES,
            rhi::PipelineStatisticFlagBits::FS_INVOCATIONS,
            rhi::PipelineStatisticFlagBits::CS_INVOCATIONS,
        };
        uint32_t count = 0;
        res.value = 0;
        res = supportedPipelineStatistics & val;
        for (auto &support : supportedFlags) {
            if (res & support) {
                ++count;
            }
        }
        return count;
    }
}
