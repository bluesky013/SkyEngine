//
// Created by Zach Lee on 2023/5/28.
//

#pragma once

#include <rhi/CommandBuffer.h>
#include <mtl/DevObject.h>
#include <mtl/RenderPass.h>
#include <mtl/FrameBuffer.h>
#include <mtl/VertexAssembly.h>
#import <Metal/MTLCommandBuffer.h>
#import <Metal/MTLRenderCommandEncoder.h>

namespace sky::mtl {
    class Device;
    class CommandBuffer;

    class GraphicsEncoder : public rhi::GraphicsEncoder {
    public:
        GraphicsEncoder(CommandBuffer &cmd) : commandBuffer(cmd) {}
        ~GraphicsEncoder() = default;

        rhi::GraphicsEncoder &BeginQuery(const rhi::QueryPoolPtr &query, uint32_t id) override;
        rhi::GraphicsEncoder &EndQuery(const rhi::QueryPoolPtr &query, uint32_t id) override;
        rhi::GraphicsEncoder &WriteTimeStamp(const rhi::QueryPoolPtr &query, rhi::PipelineStageBit stage, uint32_t id) override;
        rhi::GraphicsEncoder &BeginPass(const rhi::PassBeginInfo &beginInfo) override;
        rhi::GraphicsEncoder &BindPipeline(const rhi::GraphicsPipelinePtr &pso) override;
        rhi::GraphicsEncoder &BindAssembly(const rhi::VertexAssemblyPtr &assembly) override;
        rhi::GraphicsEncoder &SetViewport(uint32_t count, const rhi::Viewport *viewport) override;
        rhi::GraphicsEncoder &SetScissor(uint32_t count, const rhi::Rect2D *scissor) override;
        rhi::GraphicsEncoder &DrawIndexed(const rhi::CmdDrawIndexed &indexed) override;
        rhi::GraphicsEncoder &DrawLinear(const rhi::CmdDrawLinear &linear) override;
        rhi::GraphicsEncoder &DrawIndexedIndirect(const rhi::BufferPtr &buffer, uint32_t offset, uint32_t count, uint32_t stride) override;
        rhi::GraphicsEncoder &DrawIndirect(const rhi::BufferPtr &buffer, uint32_t offset, uint32_t count, uint32_t stride) override;
        rhi::GraphicsEncoder &BindSet(uint32_t id, const rhi::DescriptorSetPtr &set) override;
        rhi::GraphicsEncoder &NextSubPass() override;
        rhi::GraphicsEncoder &EndPass() override;

    private:
        CommandBuffer &commandBuffer;
        RenderPassPtr currentRenderPass;
        FrameBufferPtr currentFramebuffer;
        VertexAssemblyPtr currentVa;
        id<MTLRenderCommandEncoder> encoder;
        MTLPrimitiveType primitive = MTLPrimitiveTypeTriangle;
        MTLRenderPassDescriptor *passDesc = nil;
        uint32_t currentSubpass = 0;
    };

    class BlitEncoder : public rhi::BlitEncoder {
    public:
        BlitEncoder(CommandBuffer &cmd);
        ~BlitEncoder() override = default;

        rhi::BlitEncoder &CopyTexture() override;
        rhi::BlitEncoder &CopyTextureToBuffer() override;
        rhi::BlitEncoder &CopyBufferToTexture() override;
        rhi::BlitEncoder &BlitTexture(const rhi::ImagePtr &src, const rhi::ImagePtr &dst,
                                      const std::vector<rhi::BlitInfo> &blitInputs, rhi::Filter filter) override;
        rhi::BlitEncoder &ResoleTexture(const rhi::ImagePtr &src, const rhi::ImagePtr &dst,
                                        const std::vector<rhi::ResolveInfo> &resolveInputs) override;

    private:
        id<MTLBlitCommandEncoder> encoder = nil;
        CommandBuffer &commandBuffer;
    };

    class CommandBuffer : public rhi::CommandBuffer, public DevObject {
    public:
        CommandBuffer(Device &dev) : DevObject(dev) {}
        ~CommandBuffer();

        void Begin() override;
        void End() override;
        void Submit(rhi::Queue &queue, const rhi::SubmitInfo &submit) override;

        void ResetQueryPool(const rhi::QueryPoolPtr &queryPool, uint32_t first, uint32_t count) override;
        void GetQueryResult(const rhi::QueryPoolPtr &queryPool, uint32_t first, uint32_t count,
            const rhi::BufferPtr &result, uint32_t offset, uint32_t stride) override;
        std::shared_ptr<rhi::GraphicsEncoder> EncodeGraphics() override { return std::make_shared<GraphicsEncoder>(*this); }
        std::shared_ptr<rhi::BlitEncoder> EncodeBlit() override { return std::make_shared<BlitEncoder>(*this); }
        id<MTLCommandBuffer> GetNativeHandle() const { return currentCommandBuffer; }

    private:
        friend class Device;
        bool Init(const Descriptor &desc);

        id<MTLCommandQueue> queue = nil;
        id<MTLCommandBuffer> currentCommandBuffer = nil;
        NSAutoreleasePool *releasePool = nil;
    };

} // namespace sky::mtl
