//
// Created on 2026/04/07.
//

#import <Metal/Metal.h>

#include <MetalEncoder.h>
#include <MetalDevice.h>
#include <MetalBuffer.h>
#include <MetalImage.h>
#include <MetalPipelineState.h>
#include <aurora/rhi/Core.h>

namespace sky::aurora {

    static MTLLoadAction FromLoadOp(LoadOp op)
    {
        switch (op) {
            case LoadOp::LOAD:      return MTLLoadActionLoad;
            case LoadOp::CLEAR:     return MTLLoadActionClear;
            default:                return MTLLoadActionDontCare;
        }
    }

    static MTLStoreAction FromStoreOp(StoreOp op)
    {
        switch (op) {
            case StoreOp::STORE: return MTLStoreActionStore;
            default:             return MTLStoreActionDontCare;
        }
    }

    static MTLIndexType FromIndexType(IndexType type)
    {
        return (type == IndexType::U32) ? MTLIndexTypeUInt32 : MTLIndexTypeUInt16;
    }

    static MTLPrimitiveType FromPrimitiveTopology(PrimitiveTopology topo)
    {
        switch (topo) {
            case PrimitiveTopology::POINT_LIST:     return MTLPrimitiveTypePoint;
            case PrimitiveTopology::LINE_LIST:       return MTLPrimitiveTypeLine;
            case PrimitiveTopology::LINE_STRIP:      return MTLPrimitiveTypeLineStrip;
            case PrimitiveTopology::TRIANGLE_LIST:   return MTLPrimitiveTypeTriangle;
            case PrimitiveTopology::TRIANGLE_STRIP:  return MTLPrimitiveTypeTriangleStrip;
            default:                                 return MTLPrimitiveTypeTriangle;
        }
    }

    // ---- MetalGraphicsEncoder ----

    MetalGraphicsEncoder::MetalGraphicsEncoder(MetalDevice &device, void *cmdBuffer)
        : device(device)
        , cmdBuffer(cmdBuffer)
    {
    }

    MetalGraphicsEncoder::~MetalGraphicsEncoder()
    {
        if (renderEncoder != nullptr) {
            EndRendering();
        }
    }

    void MetalGraphicsEncoder::BeginRendering(const RenderingInfo &info)
    {
        MTLRenderPassDescriptor *rpDesc = [MTLRenderPassDescriptor renderPassDescriptor];

        for (uint32_t i = 0; i < info.numColors; ++i) {
            auto &src = info.colors[i];
            id<MTLTexture> tex = (__bridge id<MTLTexture>)static_cast<MetalImage *>(src.image)->GetNativeHandle();
            rpDesc.colorAttachments[i].texture   = tex;
            rpDesc.colorAttachments[i].loadAction  = FromLoadOp(src.loadOp);
            rpDesc.colorAttachments[i].storeAction = FromStoreOp(src.storeOp);
            if (src.loadOp == LoadOp::CLEAR) {
                const auto &c = src.clearValue.color;
                rpDesc.colorAttachments[i].clearColor = MTLClearColorMake(c.float32[0], c.float32[1], c.float32[2], c.float32[3]);
            }
        }

        if (info.depthStencil.image != nullptr) {
            id<MTLTexture> dsTex = (__bridge id<MTLTexture>)static_cast<MetalImage *>(info.depthStencil.image)->GetNativeHandle();
            rpDesc.depthAttachment.texture     = dsTex;
            rpDesc.depthAttachment.loadAction  = FromLoadOp(info.depthStencil.depthLoadOp);
            rpDesc.depthAttachment.storeAction = FromStoreOp(info.depthStencil.depthStoreOp);
            if (info.depthStencil.depthLoadOp == LoadOp::CLEAR) {
                rpDesc.depthAttachment.clearDepth = info.depthStencil.clearValue.depthStencil.depth;
            }
        }

        id<MTLCommandBuffer> cb = (__bridge id<MTLCommandBuffer>)cmdBuffer;
        renderEncoder = (__bridge_retained void *)[cb renderCommandEncoderWithDescriptor:rpDesc];
    }

    void MetalGraphicsEncoder::EndRendering()
    {
        if (renderEncoder != nullptr) {
            id<MTLRenderCommandEncoder> enc = (__bridge_transfer id<MTLRenderCommandEncoder>)renderEncoder;
            [enc endEncoding];
            renderEncoder = nullptr;
        }
    }

    void MetalGraphicsEncoder::BindPipeline(GraphicsPipeline *pso)
    {
        auto *mtlPso = static_cast<MetalGraphicsPipeline *>(pso);
        id<MTLRenderCommandEncoder> enc = (__bridge id<MTLRenderCommandEncoder>)renderEncoder;
        id<MTLRenderPipelineState> state = (__bridge id<MTLRenderPipelineState>)mtlPso->GetNativeHandle();
        [enc setRenderPipelineState:state];
    }

    void MetalGraphicsEncoder::BindResourceGroup(uint32_t /*set*/, ResourceGroup * /*group*/)
    {
        // TODO: implement once ResourceGroup maps to Metal argument buffers
    }

    void MetalGraphicsEncoder::BindVertexBuffers(uint32_t firstBinding, uint32_t count, const BufferView *views)
    {
        id<MTLRenderCommandEncoder> enc = (__bridge id<MTLRenderCommandEncoder>)renderEncoder;
        for (uint32_t i = 0; i < count; ++i) {
            auto *buf = static_cast<MetalBuffer *>(views[i].buffer);
            id<MTLBuffer> mtlBuf = (__bridge id<MTLBuffer>)buf->GetNativeHandle();
            [enc setVertexBuffer:mtlBuf offset:(NSUInteger)views[i].offset atIndex:firstBinding + i];
        }
    }

    void MetalGraphicsEncoder::BindIndexBuffer(Buffer *buffer, uint64_t offset, IndexType type)
    {
        indexBuffer = static_cast<MetalBuffer *>(buffer)->GetNativeHandle();
        indexOffset = offset;
        indexType   = static_cast<uint32_t>(FromIndexType(type));
    }

    void MetalGraphicsEncoder::SetViewport(uint32_t count, const Viewport *viewports)
    {
        if (count == 0) return;
        id<MTLRenderCommandEncoder> enc = (__bridge id<MTLRenderCommandEncoder>)renderEncoder;
        MTLViewport vp;
        vp.originX = viewports[0].x;
        vp.originY = viewports[0].y;
        vp.width   = viewports[0].width;
        vp.height  = viewports[0].height;
        vp.znear   = viewports[0].minDepth;
        vp.zfar    = viewports[0].maxDepth;
        [enc setViewport:vp];
    }

    void MetalGraphicsEncoder::SetScissor(uint32_t count, const Rect2D *scissors)
    {
        if (count == 0) return;
        id<MTLRenderCommandEncoder> enc = (__bridge id<MTLRenderCommandEncoder>)renderEncoder;
        MTLScissorRect sc;
        sc.x      = scissors[0].offset.x;
        sc.y      = scissors[0].offset.y;
        sc.width  = scissors[0].extent.width;
        sc.height = scissors[0].extent.height;
        [enc setScissorRect:sc];
    }

    void MetalGraphicsEncoder::Draw(const CmdDrawLinear &cmd)
    {
        id<MTLRenderCommandEncoder> enc = (__bridge id<MTLRenderCommandEncoder>)renderEncoder;
        [enc drawPrimitives:MTLPrimitiveTypeTriangle
                vertexStart:cmd.firstVertex
                vertexCount:cmd.vertexCount
              instanceCount:cmd.instanceCount
               baseInstance:cmd.firstInstance];
    }

    void MetalGraphicsEncoder::DrawIndexed(const CmdDrawIndexed &cmd)
    {
        id<MTLRenderCommandEncoder> enc = (__bridge id<MTLRenderCommandEncoder>)renderEncoder;
        id<MTLBuffer> ib = (__bridge id<MTLBuffer>)indexBuffer;
        MTLIndexType mtlIndexType = static_cast<MTLIndexType>(indexType);
        uint32_t indexStride = (mtlIndexType == MTLIndexTypeUInt32) ? 4 : 2;

        [enc drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                        indexCount:cmd.indexCount
                         indexType:mtlIndexType
                       indexBuffer:ib
                 indexBufferOffset:indexOffset + cmd.firstIndex * indexStride
                     instanceCount:cmd.instanceCount
                        baseVertex:cmd.vertexOffset
                      baseInstance:cmd.firstInstance];
    }

    void MetalGraphicsEncoder::DrawIndirect(Buffer *buffer, uint64_t offset, uint32_t drawCount, uint32_t /*stride*/)
    {
        id<MTLRenderCommandEncoder> enc = (__bridge id<MTLRenderCommandEncoder>)renderEncoder;
        id<MTLBuffer> indirectBuf = (__bridge id<MTLBuffer>)static_cast<MetalBuffer *>(buffer)->GetNativeHandle();
        for (uint32_t i = 0; i < drawCount; ++i) {
            [enc drawPrimitives:MTLPrimitiveTypeTriangle
                 indirectBuffer:indirectBuf
           indirectBufferOffset:offset + i * sizeof(MTLDrawPrimitivesIndirectArguments)];
        }
    }

    void MetalGraphicsEncoder::DrawIndexedIndirect(Buffer *buffer, uint64_t offset, uint32_t drawCount, uint32_t /*stride*/)
    {
        id<MTLRenderCommandEncoder> enc = (__bridge id<MTLRenderCommandEncoder>)renderEncoder;
        id<MTLBuffer> ib = (__bridge id<MTLBuffer>)indexBuffer;
        id<MTLBuffer> indirectBuf = (__bridge id<MTLBuffer>)static_cast<MetalBuffer *>(buffer)->GetNativeHandle();
        MTLIndexType mtlIndexType = static_cast<MTLIndexType>(indexType);

        for (uint32_t i = 0; i < drawCount; ++i) {
            [enc drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                             indexType:mtlIndexType
                           indexBuffer:ib
                     indexBufferOffset:indexOffset
                        indirectBuffer:indirectBuf
                  indirectBufferOffset:offset + i * sizeof(MTLDrawIndexedPrimitivesIndirectArguments)];
        }
    }

    // ---- MetalComputeEncoder ----

    MetalComputeEncoder::MetalComputeEncoder(MetalDevice &device, void *cmdBuffer)
        : device(device)
        , cmdBuffer(cmdBuffer)
    {
        id<MTLCommandBuffer> cb = (__bridge id<MTLCommandBuffer>)cmdBuffer;
        computeEncoder = (__bridge_retained void *)[cb computeCommandEncoder];
    }

    MetalComputeEncoder::~MetalComputeEncoder()
    {
        if (computeEncoder != nullptr) {
            id<MTLComputeCommandEncoder> enc = (__bridge_transfer id<MTLComputeCommandEncoder>)computeEncoder;
            [enc endEncoding];
            computeEncoder = nullptr;
        }
    }

    void MetalComputeEncoder::BindPipeline(ComputePipeline *pso)
    {
        auto *mtlPso = static_cast<MetalComputePipeline *>(pso);
        id<MTLComputeCommandEncoder> enc = (__bridge id<MTLComputeCommandEncoder>)computeEncoder;
        id<MTLComputePipelineState> state = (__bridge id<MTLComputePipelineState>)mtlPso->GetNativeHandle();
        [enc setComputePipelineState:state];
    }

    void MetalComputeEncoder::BindResourceGroup(uint32_t /*set*/, ResourceGroup * /*group*/)
    {
        // TODO: implement once ResourceGroup maps to Metal argument buffers
    }

    void MetalComputeEncoder::Dispatch(uint32_t groupX, uint32_t groupY, uint32_t groupZ)
    {
        id<MTLComputeCommandEncoder> enc = (__bridge id<MTLComputeCommandEncoder>)computeEncoder;
        // Default threadgroup size; should be derived from pipeline reflection in production
        MTLSize threadsPerGroup = MTLSizeMake(1, 1, 1);
        MTLSize threadgroups    = MTLSizeMake(groupX, groupY, groupZ);
        [enc dispatchThreadgroups:threadgroups threadsPerThreadgroup:threadsPerGroup];
    }

    void MetalComputeEncoder::DispatchIndirect(Buffer *buffer, uint64_t offset)
    {
        id<MTLComputeCommandEncoder> enc = (__bridge id<MTLComputeCommandEncoder>)computeEncoder;
        id<MTLBuffer> indirectBuf = (__bridge id<MTLBuffer>)static_cast<MetalBuffer *>(buffer)->GetNativeHandle();
        MTLSize threadsPerGroup = MTLSizeMake(1, 1, 1);
        [enc dispatchThreadgroupsWithIndirectBuffer:indirectBuf
                               indirectBufferOffset:(NSUInteger)offset
                              threadsPerThreadgroup:threadsPerGroup];
    }

    // ---- MetalBlitEncoder ----

    MetalBlitEncoder::MetalBlitEncoder(MetalDevice &device, void *cmdBuffer)
        : device(device)
        , cmdBuffer(cmdBuffer)
    {
        id<MTLCommandBuffer> cb = (__bridge id<MTLCommandBuffer>)cmdBuffer;
        blitEncoder = (__bridge_retained void *)[cb blitCommandEncoder];
    }

    MetalBlitEncoder::~MetalBlitEncoder()
    {
        if (blitEncoder != nullptr) {
            id<MTLBlitCommandEncoder> enc = (__bridge_transfer id<MTLBlitCommandEncoder>)blitEncoder;
            [enc endEncoding];
            blitEncoder = nullptr;
        }
    }

    void MetalBlitEncoder::CopyBuffer(Buffer *src, Buffer *dst, uint64_t size, uint64_t srcOffset, uint64_t dstOffset)
    {
        id<MTLBlitCommandEncoder> enc = (__bridge id<MTLBlitCommandEncoder>)blitEncoder;
        id<MTLBuffer> srcBuf = (__bridge id<MTLBuffer>)static_cast<MetalBuffer *>(src)->GetNativeHandle();
        id<MTLBuffer> dstBuf = (__bridge id<MTLBuffer>)static_cast<MetalBuffer *>(dst)->GetNativeHandle();
        [enc copyFromBuffer:srcBuf sourceOffset:(NSUInteger)srcOffset
                   toBuffer:dstBuf destinationOffset:(NSUInteger)dstOffset
                       size:(NSUInteger)size];
    }

    void MetalBlitEncoder::CopyBufferToImage(Buffer *src, Image *dst, const std::vector<BufferImageCopy> &regions)
    {
        id<MTLBlitCommandEncoder> enc = (__bridge id<MTLBlitCommandEncoder>)blitEncoder;
        id<MTLBuffer> srcBuf = (__bridge id<MTLBuffer>)static_cast<MetalBuffer *>(src)->GetNativeHandle();
        id<MTLTexture> dstTex = (__bridge id<MTLTexture>)static_cast<MetalImage *>(dst)->GetNativeHandle();

        for (const auto &region : regions) {
            MTLSize sourceSize = MTLSizeMake(region.imageExtent.width, region.imageExtent.height, region.imageExtent.depth);
            MTLOrigin dstOrigin = MTLOriginMake(region.imageOffset.x, region.imageOffset.y, region.imageOffset.z);

            NSUInteger bytesPerRow = 0;
            if (region.bufferRowLength > 0) {
                bytesPerRow = region.bufferRowLength;
            }

            NSUInteger bytesPerImage = 0;
            if (region.bufferImageHeight > 0) {
                bytesPerImage = region.bufferImageHeight * bytesPerRow;
            }

            [enc copyFromBuffer:srcBuf
                   sourceOffset:(NSUInteger)region.bufferOffset
              sourceBytesPerRow:bytesPerRow
            sourceBytesPerImage:bytesPerImage
                     sourceSize:sourceSize
                      toTexture:dstTex
               destinationSlice:region.subRange.baseLayer
               destinationLevel:region.subRange.level
              destinationOrigin:dstOrigin];
        }
    }

    void MetalBlitEncoder::CopyImageToBuffer(Image *src, Buffer *dst, const std::vector<BufferImageCopy> &regions)
    {
        id<MTLBlitCommandEncoder> enc = (__bridge id<MTLBlitCommandEncoder>)blitEncoder;
        id<MTLTexture> srcTex = (__bridge id<MTLTexture>)static_cast<MetalImage *>(src)->GetNativeHandle();
        id<MTLBuffer> dstBuf = (__bridge id<MTLBuffer>)static_cast<MetalBuffer *>(dst)->GetNativeHandle();

        for (const auto &region : regions) {
            MTLSize sourceSize = MTLSizeMake(region.imageExtent.width, region.imageExtent.height, region.imageExtent.depth);
            MTLOrigin srcOrigin = MTLOriginMake(region.imageOffset.x, region.imageOffset.y, region.imageOffset.z);

            NSUInteger bytesPerRow = 0;
            if (region.bufferRowLength > 0) {
                bytesPerRow = region.bufferRowLength;
            }

            NSUInteger bytesPerImage = 0;
            if (region.bufferImageHeight > 0) {
                bytesPerImage = region.bufferImageHeight * bytesPerRow;
            }

            [enc copyFromTexture:srcTex
                     sourceSlice:region.subRange.baseLayer
                     sourceLevel:region.subRange.level
                    sourceOrigin:srcOrigin
                      sourceSize:sourceSize
                        toBuffer:dstBuf
               destinationOffset:(NSUInteger)region.bufferOffset
          destinationBytesPerRow:bytesPerRow
        destinationBytesPerImage:bytesPerImage];
        }
    }

    void MetalBlitEncoder::BlitImage(Image * /*src*/, Image * /*dst*/, const std::vector<BlitInfo> & /*regions*/, Filter /*filter*/)
    {
        // Metal has no direct blit-with-filter equivalent on MTLBlitCommandEncoder.
        // Scaling blits require a render pass with a fragment shader.
        // TODO: implement via render-based blit helper
    }

    void MetalBlitEncoder::ResolveImage(Image * /*src*/, Image * /*dst*/, const std::vector<ResolveInfo> & /*regions*/)
    {
        // Metal MSAA resolve is typically done through MTLStoreActionMultisampleResolve
        // on the render pass attachment. Explicit resolve via blit is not directly
        // supported. TODO: implement if needed.
    }

} // namespace sky::aurora
