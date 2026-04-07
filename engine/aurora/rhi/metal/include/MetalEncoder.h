//
// Created on 2026/04/07.
//

#pragma once

#include <aurora/rhi/Encoder.h>

namespace sky::aurora {

    class MetalDevice;

    // Metal encoders wrap MTLRenderCommandEncoder / MTLComputeCommandEncoder /
    // MTLBlitCommandEncoder, but expose them as void* to keep Metal.h out of
    // the public header (Obj-C types live in the .mm translation unit).

    class MetalGraphicsEncoder : public GraphicsEncoder {
    public:
        MetalGraphicsEncoder(MetalDevice &device, void *cmdBuffer);
        ~MetalGraphicsEncoder() override;

        void BeginRendering(const RenderingInfo &info) override;
        void EndRendering() override;

        void BindPipeline(GraphicsPipeline *pso) override;
        void BindResourceGroup(uint32_t set, ResourceGroup *group) override;
        void BindVertexBuffers(uint32_t firstBinding, uint32_t count, const BufferView *views) override;
        void BindIndexBuffer(Buffer *buffer, uint64_t offset, IndexType type) override;

        void SetViewport(uint32_t count, const Viewport *viewports) override;
        void SetScissor(uint32_t count, const Rect2D *scissors) override;

        void Draw(const CmdDrawLinear &cmd) override;
        void DrawIndexed(const CmdDrawIndexed &cmd) override;
        void DrawIndirect(Buffer *buffer, uint64_t offset, uint32_t drawCount, uint32_t stride) override;
        void DrawIndexedIndirect(Buffer *buffer, uint64_t offset, uint32_t drawCount, uint32_t stride) override;

    private:
        MetalDevice &device;
        void        *cmdBuffer     = nullptr; // id<MTLCommandBuffer>
        void        *renderEncoder = nullptr; // id<MTLRenderCommandEncoder>
        void        *indexBuffer   = nullptr; // id<MTLBuffer>
        uint64_t     indexOffset   = 0;
        uint32_t     indexType     = 0;       // MTLIndexType
    };

    class MetalComputeEncoder : public ComputeEncoder {
    public:
        MetalComputeEncoder(MetalDevice &device, void *cmdBuffer);
        ~MetalComputeEncoder() override;

        void BindPipeline(ComputePipeline *pso) override;
        void BindResourceGroup(uint32_t set, ResourceGroup *group) override;
        void Dispatch(uint32_t groupX, uint32_t groupY, uint32_t groupZ) override;
        void DispatchIndirect(Buffer *buffer, uint64_t offset) override;

    private:
        MetalDevice &device;
        void        *cmdBuffer      = nullptr; // id<MTLCommandBuffer>
        void        *computeEncoder = nullptr; // id<MTLComputeCommandEncoder>
    };

    class MetalBlitEncoder : public BlitEncoder {
    public:
        MetalBlitEncoder(MetalDevice &device, void *cmdBuffer);
        ~MetalBlitEncoder() override;

        void CopyBuffer(Buffer *src, Buffer *dst, uint64_t size, uint64_t srcOffset, uint64_t dstOffset) override;
        void CopyBufferToImage(Buffer *src, Image *dst, const std::vector<BufferImageCopy> &regions) override;
        void CopyImageToBuffer(Image *src, Buffer *dst, const std::vector<BufferImageCopy> &regions) override;
        void BlitImage(Image *src, Image *dst, const std::vector<BlitInfo> &regions, Filter filter) override;
        void ResolveImage(Image *src, Image *dst, const std::vector<ResolveInfo> &regions) override;

    private:
        MetalDevice &device;
        void        *cmdBuffer   = nullptr; // id<MTLCommandBuffer>
        void        *blitEncoder = nullptr; // id<MTLBlitCommandEncoder>
    };

} // namespace sky::aurora
