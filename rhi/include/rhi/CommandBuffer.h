//
// Created by Zach Lee on 2022/11/10.
//

#pragma once

#include <rhi/Commands.h>

namespace sky::rhi {

    class ComputeEncoder {
    public:
        ComputeEncoder() = default;
        virtual ~ComputeEncoder() = default;

//        void BindComputePipeline(const ComputePipelinePtr &pso);
//        void Dispatch(uint32_t x, uint32_t y, uint32_t z);
    };

    class GraphicsEncoder {
    public:
        GraphicsEncoder() = default;
        virtual ~GraphicsEncoder() = default;

        virtual void BeginPass(const rhi::FrameBufferPtr &frameBuffer, const rhi::RenderPassPtr &renderPass, uint32_t clearCount, rhi::ClearValue *clearValues) = 0;
        virtual void BindPipeline(const GraphicsPipelinePtr &pso) = 0;
        virtual void BindAssembly(const VertexAssemblyPtr &assembly) = 0;
        virtual void SetViewport(uint32_t count, const Viewport *viewport) = 0;
        virtual void SetScissor(uint32_t count, const Rect2D *scissor) = 0;
        virtual void DrawIndexed(const CmdDrawIndexed &indexed) = 0;
        virtual void DrawLinear(const CmdDrawLinear &linear) = 0;
        virtual void DrawIndirect(const BufferPtr &buffer, uint32_t offset, uint32_t size) = 0;
        virtual void EndPass() = 0;
    };

    class CommandBuffer {
    public:
        CommandBuffer() = default;
        virtual ~CommandBuffer() = default;

        struct Descriptor {
        };

        virtual std::shared_ptr<GraphicsEncoder> EncodeGraphics() { return {}; }
    };

}
