//
// Created by Zach Lee on 2026/3/30.
//

#pragma once

#include <aurora/rhi/Core.h>
#include <cstdint>
#include <vector>

namespace sky::aurora {

    class Buffer;
    class Image;
    class GraphicsPipeline;
    class ComputePipeline;
    class ResourceGroup;

    class GraphicsEncoder {
    public:
        GraphicsEncoder() = default;
        virtual ~GraphicsEncoder() = default;

        // render pass (dynamic rendering)
        virtual void BeginRendering(const RenderingInfo &info) = 0;
        virtual void EndRendering() = 0;

        // pipeline
        virtual void BindPipeline(GraphicsPipeline *pso) = 0;

        // resource binding
        virtual void BindResourceGroup(uint32_t set, ResourceGroup *group) = 0;

        // vertex / index
        virtual void BindVertexBuffers(uint32_t firstBinding, uint32_t count, const BufferView *views) = 0;
        virtual void BindIndexBuffer(Buffer *buffer, uint64_t offset, IndexType type) = 0;

        // dynamic state
        virtual void SetViewport(uint32_t count, const Viewport *viewports) = 0;
        virtual void SetScissor(uint32_t count, const Rect2D *scissors) = 0;

        // draw
        virtual void Draw(const CmdDrawLinear &cmd) = 0;
        virtual void DrawIndexed(const CmdDrawIndexed &cmd) = 0;
        virtual void DrawIndirect(Buffer *buffer, uint64_t offset, uint32_t drawCount, uint32_t stride) = 0;
        virtual void DrawIndexedIndirect(Buffer *buffer, uint64_t offset, uint32_t drawCount, uint32_t stride) = 0;
    };

    class ComputeEncoder {
    public:
        ComputeEncoder() = default;
        virtual ~ComputeEncoder() = default;

        virtual void BindPipeline(ComputePipeline *pso) = 0;
        virtual void BindResourceGroup(uint32_t set, ResourceGroup *group) = 0;
        virtual void Dispatch(uint32_t groupX, uint32_t groupY, uint32_t groupZ) = 0;
        virtual void DispatchIndirect(Buffer *buffer, uint64_t offset) = 0;
    };

    class BlitEncoder {
    public:
        BlitEncoder() = default;
        virtual ~BlitEncoder() = default;

        // buffer copy
        virtual void CopyBuffer(Buffer *src, Buffer *dst, uint64_t size, uint64_t srcOffset, uint64_t dstOffset) = 0;

        // image copy
        virtual void CopyBufferToImage(Buffer *src, Image *dst, const std::vector<BufferImageCopy> &regions) = 0;
        virtual void CopyImageToBuffer(Image *src, Buffer *dst, const std::vector<BufferImageCopy> &regions) = 0;

        // blit / resolve
        virtual void BlitImage(Image *src, Image *dst, const std::vector<BlitInfo> &regions, Filter filter) = 0;
        virtual void ResolveImage(Image *src, Image *dst, const std::vector<ResolveInfo> &regions) = 0;
    };

} // namespace sky::aurora