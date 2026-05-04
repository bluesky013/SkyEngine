//
// Created on 2026/04/07.
//

#import <Metal/Metal.h>
#include <MetalCommandPool.h>
#include <MetalDevice.h>
#include <MetalEncoder.h>
#include <aurora/rhi/Core.h>

namespace sky::aurora {

    namespace {
        MTLBarrierScope ScopeForBarrierInfo(const BarrierInfo &info)
        {
            MTLBarrierScope s = (MTLBarrierScope)0;
            if (!info.bufferBarriers.empty() || !info.memoryBarriers.empty()) {
                s |= MTLBarrierScopeBuffers;
            }
            if (!info.imageBarriers.empty() || !info.memoryBarriers.empty()) {
                s |= MTLBarrierScopeTextures;
            }
            return s;
        }

        MTLRenderStages StagesForRender(const PipelineStageFlags &flags)
        {
            MTLRenderStages r = (MTLRenderStages)0;
            if (flags & PipelineStageBit::VERTEX_SHADER) {
                r |= MTLRenderStageVertex;
            }
            if (flags & PipelineStageBit::FRAGMENT_SHADER) {
                r |= MTLRenderStageFragment;
            }
            // top/bottom/color-output etc. roll into both stages
            if (flags & (PipelineStageBit::TOP | PipelineStageBit::COLOR_OUTPUT |
                        PipelineStageBit::EARLY_FRAGMENT | PipelineStageBit::LATE_FRAGMENT)) {
                r |= MTLRenderStageFragment;
            }
            if (r == (MTLRenderStages)0) {
                r = MTLRenderStageVertex | MTLRenderStageFragment;
            }
            return r;
        }
    }

    // ---- MetalCommandBuffer ----

    MetalCommandBuffer::MetalCommandBuffer(MetalDevice &dev, void *q)
        : device(dev)
        , queue(q)
    {
    }

    MetalCommandBuffer::~MetalCommandBuffer()
    {
        if (cmdBuffer != nullptr) {
            id<MTLCommandBuffer> cb = (__bridge_transfer id<MTLCommandBuffer>)cmdBuffer;
            cb = nil;
            cmdBuffer = nullptr;
        }
    }

    void MetalCommandBuffer::Begin()
    {
        if (cmdBuffer != nullptr) {
            id<MTLCommandBuffer> old = (__bridge_transfer id<MTLCommandBuffer>)cmdBuffer;
            old = nil;
            cmdBuffer = nullptr;
        }
        id<MTLCommandQueue> mtlQueue = (__bridge id<MTLCommandQueue>)queue;
        id<MTLCommandBuffer> cb = [mtlQueue commandBuffer];
        cmdBuffer = (__bridge_retained void *)cb;
    }

    void MetalCommandBuffer::End()
    {
        // Metal command buffer commit happens at submit time, not here.
    }

    void MetalCommandBuffer::PipelineBarrier(const BarrierInfo &info)
    {
        // No-op fast path: empty BarrierInfo.
        if (info.imageBarriers.empty() && info.bufferBarriers.empty() && info.memoryBarriers.empty()) {
            return;
        }

        if (activeEncoder == nullptr) {
            // No encoder yet; defer until next CreateXxxEncoder.
            pendingBarriers.push_back(info);
            return;
        }

        // Route to active encoder. Layout transitions are no-ops on Metal;
        // we only emit memory ordering.
        const MTLBarrierScope scope = ScopeForBarrierInfo(info);

        switch (activeKind) {
        case ActiveEncoderKind::Render: {
            id<MTLRenderCommandEncoder> e = (__bridge id<MTLRenderCommandEncoder>)activeEncoder;
            const MTLRenderStages after  = StagesForRender(info.srcStage);
            const MTLRenderStages before = StagesForRender(info.dstStage);
            [e memoryBarrierWithScope:scope afterStages:after beforeStages:before];
            break;
        }
        case ActiveEncoderKind::Compute: {
            id<MTLComputeCommandEncoder> e = (__bridge id<MTLComputeCommandEncoder>)activeEncoder;
            [e memoryBarrierWithScope:scope];
            break;
        }
        case ActiveEncoderKind::Blit:
            // Blit encoders rely on implicit between-encoder sync; no API.
            break;
        case ActiveEncoderKind::None:
            break;
        }
    }

    void MetalCommandBuffer::NotifyEncoderBegin(ActiveEncoderKind kind, void *encoder)
    {
        activeEncoder = encoder;
        activeKind    = kind;

        // Flush pending barriers onto the new encoder.
        if (!pendingBarriers.empty() && encoder != nullptr) {
            std::vector<BarrierInfo> drained;
            drained.swap(pendingBarriers);
            for (const auto &b : drained) {
                PipelineBarrier(b);
            }
        }
    }

    void MetalCommandBuffer::NotifyEncoderEnd()
    {
        activeEncoder = nullptr;
        activeKind    = ActiveEncoderKind::None;
    }

    std::unique_ptr<GraphicsEncoder> MetalCommandBuffer::CreateGraphicsEncoder()
    {
        return std::make_unique<MetalGraphicsEncoder>(device, this);
    }

    std::unique_ptr<ComputeEncoder> MetalCommandBuffer::CreateComputeEncoder()
    {
        return std::make_unique<MetalComputeEncoder>(device, this);
    }

    std::unique_ptr<BlitEncoder> MetalCommandBuffer::CreateBlitEncoder()
    {
        return std::make_unique<MetalBlitEncoder>(device, this);
    }

    // ---- MetalCommandPool ----

    MetalCommandPool::MetalCommandPool(MetalDevice &dev, void *q)
        : device(dev)
        , queue(q)
    {
    }

    MetalCommandPool::~MetalCommandPool()
    {
        for (auto *buffer : allocatedBuffers) {
            delete buffer;
        }
        allocatedBuffers.clear();
    }

    bool MetalCommandPool::Init()
    {
        return true;
    }

    void MetalCommandPool::Reset()
    {
    }

    CommandBuffer *MetalCommandPool::Allocate()
    {
        auto *cmdBuffer = new MetalCommandBuffer(device, queue);
        allocatedBuffers.push_back(cmdBuffer);
        return cmdBuffer;
    }

} // namespace sky::aurora
