//
// Created by Zach Lee on 2022/11/10.
//

#pragma once

#include <rhi/Commands.h>
#include <rhi/Fence.h>
#include <rhi/QueryPool.h>

namespace sky::rhi {
    class Queue;

    class ComputeEncoder {
    public:
        ComputeEncoder() = default;
        virtual ~ComputeEncoder() = default;

//        void BindComputePipeline(const ComputePipelinePtr &pso);
//        void Dispatch(uint32_t x, uint32_t y, uint32_t z);
    };

    struct PassBeginInfo {
        rhi::FrameBufferPtr frameBuffer;
        rhi::RenderPassPtr renderPass;
        uint32_t clearCount = 0;
        rhi::ClearValue *clearValues = nullptr;
        SubPassContent contents = SubPassContent::INLINE;
    };

    class GraphicsEncoder {
    public:
        GraphicsEncoder() = default;
        virtual ~GraphicsEncoder() = default;

        virtual GraphicsEncoder &BeginQuery(const QueryPoolPtr &query, uint32_t id) = 0;
        virtual GraphicsEncoder &EndQuery(const QueryPoolPtr &query, uint32_t id) = 0;
        virtual GraphicsEncoder &WriteTimeStamp(const QueryPoolPtr &query, rhi::PipelineStageBit stage, uint32_t id) = 0;
        virtual GraphicsEncoder &BeginPass(const PassBeginInfo &beginInfo) = 0;
        virtual GraphicsEncoder &BindPipeline(const GraphicsPipelinePtr &pso) = 0;
        virtual GraphicsEncoder &BindAssembly(const VertexAssemblyPtr &assembly) = 0;
        virtual GraphicsEncoder &SetViewport(uint32_t count, const Viewport *viewport) = 0;
        virtual GraphicsEncoder &SetScissor(uint32_t count, const Rect2D *scissor) = 0;
        virtual GraphicsEncoder &DrawIndexed(const CmdDrawIndexed &indexed) = 0;
        virtual GraphicsEncoder &DrawLinear(const CmdDrawLinear &linear) = 0;
        virtual GraphicsEncoder &DrawIndexedIndirect(const BufferPtr &buffer, uint32_t offset, uint32_t count, uint32_t stride) = 0;
        virtual GraphicsEncoder &DrawIndirect(const BufferPtr &buffer, uint32_t offset, uint32_t count, uint32_t stride) = 0;
        virtual GraphicsEncoder &NextSubPass() = 0;
        virtual GraphicsEncoder &EndPass() = 0;
        virtual GraphicsEncoder &BindSet(uint32_t id, const DescriptorSetPtr &set) = 0;
        virtual GraphicsEncoder &SetOffset(uint32_t set, uint32_t binding, uint32_t index, uint32_t offset) { return *this; }
    };

    class BlitEncoder {
    public:
        BlitEncoder() = default;
        virtual ~BlitEncoder() = default;

        virtual BlitEncoder &CopyTexture() = 0;
        virtual BlitEncoder &CopyTextureToBuffer() = 0;
        virtual BlitEncoder &CopyBufferToTexture() = 0;

        virtual BlitEncoder &BlitTexture(const ImagePtr &src, const ImagePtr &dst, const std::vector<BlitInfo> &blitInputs, rhi::Filter filter) = 0;
        virtual BlitEncoder &ResoleTexture(const ImagePtr &src, const ImagePtr &dst, const std::vector<ResolveInfo> &resolveInputs) = 0;

        virtual BlitEncoder &CopyBuffer(const BufferPtr &src, const BufferPtr &dst, uint64_t size, uint64_t srcOffset, uint64_t dstOffset) = 0;
    };


    struct SubmitInfo {
        std::vector<std::pair<PipelineStageFlags, SemaphorePtr>> waits;
        std::vector<SemaphorePtr> submitSignals;
        FencePtr fence;
    };

    struct BufferBarrier {
        AccessFlags srcFlags;
        AccessFlags dstFlags;
        uint32_t srcQueueFamily = (~0U);
        uint32_t dstQueueFamily = (~0U);
        BufferViewPtr view;
    };

    struct ImageBarrier {
        AccessFlags srcFlags;
        AccessFlags dstFlags;
        uint32_t srcQueueFamily = (~0U);
        uint32_t dstQueueFamily = (~0U);
        ImageViewPtr view;
    };

    struct BarrierInfo {
        AccessFlags srcFlags;
        AccessFlags dstFlags;
        uint32_t srcQueueFamily = (~0U);
        uint32_t dstQueueFamily = (~0U);
    };

    class CommandBuffer {
    public:
        CommandBuffer()          = default;
        virtual ~CommandBuffer() = default;

        struct Descriptor {
            QueueType queueType = QueueType::GRAPHICS;
        };

        virtual void Begin()                                        = 0;
        virtual void End()                                          = 0;
        virtual void Submit(Queue &queue, const SubmitInfo &submit) = 0;

        virtual std::shared_ptr<GraphicsEncoder> EncodeGraphics() = 0;
        virtual std::shared_ptr<BlitEncoder>     EncodeBlit()     = 0;

        virtual void ResetQueryPool(const QueryPoolPtr &queryPool, uint32_t first, uint32_t count) {}
        virtual void
        GetQueryResult(const QueryPoolPtr &queryPool, uint32_t first, uint32_t count, const BufferPtr &result, uint32_t offset, uint32_t stride) {}
        virtual void QueueBarrier(const ImageBarrier &imageBarrier) {}
        virtual void QueueBarrier(const ImagePtr &image, const ImageSubRange &range, const BarrierInfo &barrierInfo) {}
        virtual void QueueBarrier(const BufferPtr &buffer, uint64_t offset, uint64_t range, const BarrierInfo &barrierInfo) {}
        virtual void FlushBarriers() {}
    };
    using CommandBufferPtr = std::shared_ptr<CommandBuffer>;
}
