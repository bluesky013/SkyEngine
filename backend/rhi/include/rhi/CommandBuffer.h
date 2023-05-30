//
// Created by Zach Lee on 2022/11/10.
//

#pragma once

#include <rhi/Commands.h>

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
        SubPassContent contents;
    };

    class GraphicsEncoder {
    public:
        GraphicsEncoder() = default;
        virtual ~GraphicsEncoder() = default;

        virtual GraphicsEncoder &BeginPass(const PassBeginInfo &beginInfo) = 0;
        virtual GraphicsEncoder &BindPipeline(const GraphicsPipelinePtr &pso) = 0;
        virtual GraphicsEncoder &BindAssembly(const VertexAssemblyPtr &assembly) = 0;
        virtual GraphicsEncoder &SetViewport(uint32_t count, const Viewport *viewport) = 0;
        virtual GraphicsEncoder &SetScissor(uint32_t count, const Rect2D *scissor) = 0;
        virtual GraphicsEncoder &DrawIndexed(const CmdDrawIndexed &indexed) = 0;
        virtual GraphicsEncoder &DrawLinear(const CmdDrawLinear &linear) = 0;
        virtual GraphicsEncoder &DrawIndexedIndirect(const BufferPtr &buffer, uint32_t offset, uint32_t count, uint32_t stride) = 0;
        virtual GraphicsEncoder &DrawIndirect(const BufferPtr &buffer, uint32_t offset, uint32_t count, uint32_t stride) = 0;
        virtual GraphicsEncoder &BindSet(uint32_t id, const DescriptorSetPtr &set) = 0;
        virtual GraphicsEncoder &NextSubPass() = 0;
        virtual GraphicsEncoder &EndPass() = 0;
    };


    struct SubmitInfo {
        std::vector<std::pair<PipelineStageFlags, SemaphorePtr>> waits;
        std::vector<SemaphorePtr> submitSignals;
    };

    struct BufferBarrier {
        std::vector<AccessFlag> srcFlags;
        std::vector<AccessFlag> dstFlags;
        uint32_t srcQueueFamily = (~0U);
        uint32_t dstQueueFamily = (~0U);
        BufferViewPtr view;
    };

    struct ImageBarrier {
        std::vector<AccessFlag> srcFlags;
        std::vector<AccessFlag> dstFlags;
        uint32_t srcQueueFamily = (~0U);
        uint32_t dstQueueFamily = (~0U);
        ImageViewPtr view;
    };

    class CommandBuffer {
    public:
        CommandBuffer() = default;
        virtual ~CommandBuffer() = default;

        struct Descriptor {
            QueueType queueType = QueueType::GRAPHICS;
        };

        virtual void Begin() = 0;
        virtual void End() = 0;
        virtual void Submit(Queue &queue, const SubmitInfo &submit) = 0;

        virtual void QueueBarrier(const ImageBarrier &imageBarrier) {}
        virtual void FlushBarriers() {}

        virtual std::shared_ptr<GraphicsEncoder> EncodeGraphics() = 0;
    };
    using CommandBufferPtr = std::shared_ptr<CommandBuffer>;
}
