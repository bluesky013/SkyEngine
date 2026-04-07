//
// Created on 2026/04/07.
//

#pragma once

#include <aurora/rhi/Encoder.h>
#include <GLESForward.h>

namespace sky::aurora {

    class GLESDevice;

    class GLESGraphicsEncoder : public GraphicsEncoder {
    public:
        explicit GLESGraphicsEncoder(GLESDevice &device);
        ~GLESGraphicsEncoder() override = default;

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
        GLESDevice &device;
        GLenum      currentTopology = GL_TRIANGLES;
        GLenum      currentIndexType = GL_UNSIGNED_SHORT;
        GLuint      boundIndexBuffer = 0;
        uint64_t    indexBufferOffset = 0;
        GLuint      fbo = 0; // transient FBO for dynamic rendering
    };

    class GLESComputeEncoder : public ComputeEncoder {
    public:
        explicit GLESComputeEncoder(GLESDevice &device);
        ~GLESComputeEncoder() override = default;

        void BindPipeline(ComputePipeline *pso) override;
        void BindResourceGroup(uint32_t set, ResourceGroup *group) override;
        void Dispatch(uint32_t groupX, uint32_t groupY, uint32_t groupZ) override;
        void DispatchIndirect(Buffer *buffer, uint64_t offset) override;

    private:
        GLESDevice &device;
    };

    class GLESBlitEncoder : public BlitEncoder {
    public:
        explicit GLESBlitEncoder(GLESDevice &device);
        ~GLESBlitEncoder() override = default;

        void CopyBuffer(Buffer *src, Buffer *dst, uint64_t size, uint64_t srcOffset, uint64_t dstOffset) override;
        void CopyBufferToImage(Buffer *src, Image *dst, const std::vector<BufferImageCopy> &regions) override;
        void CopyImageToBuffer(Image *src, Buffer *dst, const std::vector<BufferImageCopy> &regions) override;
        void BlitImage(Image *src, Image *dst, const std::vector<BlitInfo> &regions, Filter filter) override;
        void ResolveImage(Image *src, Image *dst, const std::vector<ResolveInfo> &regions) override;

    private:
        GLESDevice &device;
        GLuint      readFbo  = 0;
        GLuint      writeFbo = 0;
    };

} // namespace sky::aurora
