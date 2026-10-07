//
// Created on 2026/04/07.
//

#import <Metal/Metal.h>

#include "MetalBuffer.h"
#include "MetalCommandPool.h"
#include "MetalDevice.h"
#include "MetalEncoder.h"
#include "MetalImage.h"
#include "MetalPipelineState.h"
#include "MetalResourceGroup.h"
#include "MetalShader.h"
#include "MetalUtils.h"
#include <aurora/rhi/Core.h>
#include <core/logger/Logger.h>
#include <core/platform/Platform.h>

#include "MetalBlitHelper.h"
#include "MetalPushConstantStaging.h"

#include <algorithm>

static const char *TAG = "AuroraMetal";

namespace sky::aurora {

    // ---- MetalGraphicsEncoder ----

    MetalGraphicsEncoder::MetalGraphicsEncoder(MetalDevice &device, MetalCommandBuffer *o) : device(device), owner(o)
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
            auto          &src                     = info.colors[i];
            id<MTLTexture> tex                     = (__bridge id<MTLTexture>)static_cast<MetalImage *>(src.image)->GetNativeHandle();
            rpDesc.colorAttachments[i].texture     = tex;
            rpDesc.colorAttachments[i].loadAction  = ToMetalLoadAction(src.loadOp);
            rpDesc.colorAttachments[i].storeAction = ToMetalStoreAction(src.storeOp);
            if (src.loadOp == LoadOp::CLEAR) {
                const auto &c                         = src.clearValue.color;
                rpDesc.colorAttachments[i].clearColor = MTLClearColorMake(c.float32[0], c.float32[1], c.float32[2], c.float32[3]);
            }
        }

        if (info.depthStencil.image != nullptr) {
            auto          *dsImage = static_cast<MetalImage *>(info.depthStencil.image);
            id<MTLTexture> dsTex   = (__bridge id<MTLTexture>)dsImage->GetNativeHandle();
            const auto    &fmtInfo = GetImageFormatInfo(dsImage->GetPixelFormat());

            if (fmtInfo.hasDepth) {
                rpDesc.depthAttachment.texture     = dsTex;
                rpDesc.depthAttachment.loadAction  = ToMetalLoadAction(info.depthStencil.depthLoadOp);
                rpDesc.depthAttachment.storeAction = ToMetalStoreAction(info.depthStencil.depthStoreOp);
                if (info.depthStencil.depthLoadOp == LoadOp::CLEAR) {
                    rpDesc.depthAttachment.clearDepth = info.depthStencil.clearValue.depthStencil.depth;
                }
            }

            if (fmtInfo.hasStencil) {
                rpDesc.stencilAttachment.texture     = dsTex;
                rpDesc.stencilAttachment.loadAction  = ToMetalLoadAction(info.depthStencil.stencilLoadOp);
                rpDesc.stencilAttachment.storeAction = ToMetalStoreAction(info.depthStencil.stencilStoreOp);
                if (info.depthStencil.stencilLoadOp == LoadOp::CLEAR) {
                    rpDesc.stencilAttachment.clearStencil = info.depthStencil.clearValue.depthStencil.stencil;
                }
            }
        }

        id<MTLCommandBuffer>        cb        = (__bridge id<MTLCommandBuffer>)owner->GetNativeHandle();
        id<MTLRenderCommandEncoder> nativeEnc = [cb renderCommandEncoderWithDescriptor:rpDesc];
        renderEncoder                         = (__bridge_retained void *)nativeEnc;
        owner->NotifyEncoderBegin(MetalCommandBuffer::ActiveEncoderKind::Render, (__bridge void *)nativeEnc);

        // match the DX12 contract: renderArea seeds the initial viewport+scissor
        if (info.renderArea.extent.width > 0 && info.renderArea.extent.height > 0) {
            MTLViewport vp;
            vp.originX = info.renderArea.offset.x;
            vp.originY = info.renderArea.offset.y;
            vp.width   = info.renderArea.extent.width;
            vp.height  = info.renderArea.extent.height;
            vp.znear   = 0.0;
            vp.zfar    = 1.0;
            [nativeEnc setViewport:vp];
            MTLScissorRect sc;
            sc.x      = info.renderArea.offset.x;
            sc.y      = info.renderArea.offset.y;
            sc.width  = info.renderArea.extent.width;
            sc.height = info.renderArea.extent.height;
            [nativeEnc setScissorRect:sc];
        }
    }

    void MetalGraphicsEncoder::EndRendering()
    {
        if (renderEncoder != nullptr) {
            id<MTLRenderCommandEncoder> enc = (__bridge_transfer id<MTLRenderCommandEncoder>)renderEncoder;
            [enc endEncoding];
            renderEncoder = nullptr;
            owner->NotifyEncoderEnd();
        }
    }

    void MetalGraphicsEncoder::BindPipeline(GraphicsPipeline *pso)
    {
        auto                       *mtlPso = static_cast<MetalGraphicsPipeline *>(pso);
        id<MTLRenderCommandEncoder> enc    = (__bridge id<MTLRenderCommandEncoder>)renderEncoder;
        id<MTLRenderPipelineState>  state  = (__bridge id<MTLRenderPipelineState>)mtlPso->GetNativeHandle();
        [enc setRenderPipelineState:state];
        currentPipeline = mtlPso;

        // depth/stencil + rasterizer state are baked into the pipeline object on
        // Vulkan; on Metal they are encoder state and must be applied here
        if (auto *dss = mtlPso->GetDepthStencilState(); dss != nullptr) {
            [enc setDepthStencilState:(__bridge id<MTLDepthStencilState>)dss];
            uint32_t refFront = 0, refBack = 0;
            mtlPso->GetStencilReference(refFront, refBack);
            [enc setStencilFrontReferenceValue:refFront backReferenceValue:refBack];
        }
        [enc setCullMode:ToMetalCullMode(mtlPso->GetCullMode())];
        [enc setFrontFacingWinding:ToMetalWinding(mtlPso->GetFrontFace())];
        [enc setTriangleFillMode:ToMetalFillMode(mtlPso->GetPolygonMode())];
        [enc setDepthClipMode:mtlPso->GetDepthClamp() ? MTLDepthClipModeClamp : MTLDepthClipModeClip];
        float biasConstant = 0.f, biasClamp = 0.f, biasSlope = 0.f;
        if (mtlPso->GetDepthBias(biasConstant, biasClamp, biasSlope)) {
            [enc setDepthBias:biasConstant slopeScale:biasSlope clamp:biasClamp];
        }
    }

    void MetalGraphicsEncoder::BindResourceGroup(uint32_t set, ResourceGroup *group, uint32_t numDynamicOffsets, const uint32_t *dynamicOffsets)
    {
        (void)set; // slang MSL flattens register spaces; bindings are
                   // per-category indices recorded in the group
        if (group == nullptr) {
            return;
        }
        static_cast<MetalResourceGroup *>(group)->BindGraphics(renderEncoder, numDynamicOffsets, dynamicOffsets);
    }

    void MetalGraphicsEncoder::BindDescriptorHeap(DescriptorHeap * /*heap*/)
    {
        // TODO: Metal argument buffer heap bind (aurora-resource-group tier2)
    }

    void MetalGraphicsEncoder::PushConstants(ShaderStageFlags stages, uint32_t offset, uint32_t size, const void *data)
    {
        // slang lowers push constants to a plain constant buffer occupying the
        // highest buffer slot (declare-last convention); the staging block covers
        // the partial-range accumulation (see MetalPushConstantStaging)
        if (data == nullptr || size == 0 || currentPipeline == nullptr) {
            return;
        }
        auto *shader = currentPipeline->GetShader();
        if (shader == nullptr) {
            return;
        }

        uint32_t    length  = 0;
        const void *block   = AccumulatePushConstant(pushConstantBlock, shader->GetPushConstantSize(), offset, size, data, length);
        const uint32_t slot = shader->GetPushConstantSlot();

        id<MTLRenderCommandEncoder> enc = (__bridge id<MTLRenderCommandEncoder>)renderEncoder;
        if (stages & ShaderStageFlagBit::VS) {
            [enc setVertexBytes:block length:length atIndex:slot];
        }
        if (stages & ShaderStageFlagBit::FS) {
            [enc setFragmentBytes:block length:length atIndex:slot];
        }
    }

    void MetalGraphicsEncoder::BindVertexBuffers(uint32_t firstBinding, uint32_t count, const BufferView *views)
    {
        SKY_ASSERT(firstBinding + count <= MAX_VERTEX_BINDINGS);
        id<MTLRenderCommandEncoder> enc = (__bridge id<MTLRenderCommandEncoder>)renderEncoder;
        for (uint32_t i = 0; i < count; ++i) {
            auto         *buf    = static_cast<MetalBuffer *>(views[i].buffer);
            id<MTLBuffer> mtlBuf = (__bridge id<MTLBuffer>)buf->GetNativeHandle();
            // vertex buffers live above the shader buffer range (see MetalUtils.h)
            [enc setVertexBuffer:mtlBuf offset:(NSUInteger)views[i].offset atIndex:METAL_VERTEX_BUFFER_SLOT_BASE + firstBinding + i];
        }
    }

    void MetalGraphicsEncoder::BindIndexBuffer(Buffer *buffer, uint64_t offset, IndexType type)
    {
        indexBuffer = static_cast<MetalBuffer *>(buffer)->GetNativeHandle();
        indexOffset = offset;
        indexType   = static_cast<uint32_t>(ToMetalIndexType(type));
    }

    void MetalGraphicsEncoder::SetViewport(uint32_t count, const Viewport *viewports)
    {
        SKY_ASSERT(count <= MAX_VIEWPORTS);
        if (count == 0)
            return;
        if (count > 1) {
            LOG_W(TAG, "Metal supports a single viewport; using viewport 0 of %u", count);
        }
        id<MTLRenderCommandEncoder> enc = (__bridge id<MTLRenderCommandEncoder>)renderEncoder;
        MTLViewport                 vp;
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
        SKY_ASSERT(count <= MAX_VIEWPORTS);
        if (count == 0)
            return;
        if (count > 1) {
            LOG_W(TAG, "Metal supports a single scissor rect; using rect 0 of %u", count);
        }
        id<MTLRenderCommandEncoder> enc = (__bridge id<MTLRenderCommandEncoder>)renderEncoder;
        MTLScissorRect              sc;
        sc.x      = scissors[0].offset.x;
        sc.y      = scissors[0].offset.y;
        sc.width  = scissors[0].extent.width;
        sc.height = scissors[0].extent.height;
        [enc setScissorRect:sc];
    }

    static MTLPrimitiveType TopologyOf(const MetalGraphicsPipeline *pso)
    {
        return ToMetalPrimitiveType(pso != nullptr ? pso->GetTopology() : PrimitiveTopology::TRIANGLE_LIST);
    }

    void MetalGraphicsEncoder::Draw(const CmdDrawLinear &cmd)
    {
        id<MTLRenderCommandEncoder> enc = (__bridge id<MTLRenderCommandEncoder>)renderEncoder;
        [enc drawPrimitives:TopologyOf(currentPipeline)
                vertexStart:cmd.firstVertex
                vertexCount:cmd.vertexCount
              instanceCount:cmd.instanceCount
               baseInstance:cmd.firstInstance];
    }

    void MetalGraphicsEncoder::DrawIndexed(const CmdDrawIndexed &cmd)
    {
        id<MTLRenderCommandEncoder> enc          = (__bridge id<MTLRenderCommandEncoder>)renderEncoder;
        id<MTLBuffer>               ib           = (__bridge id<MTLBuffer>)indexBuffer;
        MTLIndexType                mtlIndexType = static_cast<MTLIndexType>(indexType);
        uint32_t                    indexStride  = (mtlIndexType == MTLIndexTypeUInt32) ? 4 : 2;

        [enc drawIndexedPrimitives:TopologyOf(currentPipeline)
                        indexCount:cmd.indexCount
                         indexType:mtlIndexType
                       indexBuffer:ib
                 indexBufferOffset:indexOffset + cmd.firstIndex * indexStride
                     instanceCount:cmd.instanceCount
                        baseVertex:cmd.vertexOffset
                      baseInstance:cmd.firstInstance];
    }

    void MetalGraphicsEncoder::DrawIndirect(Buffer *buffer, uint64_t offset, uint32_t drawCount, uint32_t stride)
    {
        // Metal indirect argument structs are fixed-size; a zero stride means
        // tightly packed (same convention as Vulkan)
        const uint32_t effectiveStride = stride != 0 ? stride : sizeof(MTLDrawPrimitivesIndirectArguments);
        SKY_ASSERT(effectiveStride >= sizeof(MTLDrawPrimitivesIndirectArguments));
        id<MTLRenderCommandEncoder> enc         = (__bridge id<MTLRenderCommandEncoder>)renderEncoder;
        id<MTLBuffer>               indirectBuf = (__bridge id<MTLBuffer>)static_cast<MetalBuffer *>(buffer)->GetNativeHandle();
        for (uint32_t i = 0; i < drawCount; ++i) {
            [enc drawPrimitives:TopologyOf(currentPipeline) indirectBuffer:indirectBuf indirectBufferOffset:offset + i * effectiveStride];
        }
    }

    void MetalGraphicsEncoder::DrawIndexedIndirect(Buffer *buffer, uint64_t offset, uint32_t drawCount, uint32_t stride)
    {
        const uint32_t effectiveStride = stride != 0 ? stride : sizeof(MTLDrawIndexedPrimitivesIndirectArguments);
        SKY_ASSERT(effectiveStride >= sizeof(MTLDrawIndexedPrimitivesIndirectArguments));
        id<MTLRenderCommandEncoder> enc          = (__bridge id<MTLRenderCommandEncoder>)renderEncoder;
        id<MTLBuffer>               ib           = (__bridge id<MTLBuffer>)indexBuffer;
        id<MTLBuffer>               indirectBuf  = (__bridge id<MTLBuffer>)static_cast<MetalBuffer *>(buffer)->GetNativeHandle();
        MTLIndexType                mtlIndexType = static_cast<MTLIndexType>(indexType);

        for (uint32_t i = 0; i < drawCount; ++i) {
            [enc drawIndexedPrimitives:TopologyOf(currentPipeline)
                             indexType:mtlIndexType
                           indexBuffer:ib
                     indexBufferOffset:indexOffset
                        indirectBuffer:indirectBuf
                  indirectBufferOffset:offset + i * effectiveStride];
        }
    }

    // ---- MetalComputeEncoder ----

    MetalComputeEncoder::MetalComputeEncoder(MetalDevice &device, MetalCommandBuffer *o) : device(device), owner(o)
    {
        id<MTLCommandBuffer>         cb        = (__bridge id<MTLCommandBuffer>)owner->GetNativeHandle();
        id<MTLComputeCommandEncoder> nativeEnc = [cb computeCommandEncoder];
        computeEncoder                         = (__bridge_retained void *)nativeEnc;
        owner->NotifyEncoderBegin(MetalCommandBuffer::ActiveEncoderKind::Compute, (__bridge void *)nativeEnc);
    }

    MetalComputeEncoder::~MetalComputeEncoder()
    {
        if (computeEncoder != nullptr) {
            id<MTLComputeCommandEncoder> enc = (__bridge_transfer id<MTLComputeCommandEncoder>)computeEncoder;
            [enc endEncoding];
            computeEncoder = nullptr;
            owner->NotifyEncoderEnd();
        }
    }

    void MetalComputeEncoder::BindPipeline(ComputePipeline *pso)
    {
        auto                        *mtlPso = static_cast<MetalComputePipeline *>(pso);
        id<MTLComputeCommandEncoder> enc    = (__bridge id<MTLComputeCommandEncoder>)computeEncoder;
        id<MTLComputePipelineState>  state  = (__bridge id<MTLComputePipelineState>)mtlPso->GetNativeHandle();
        [enc setComputePipelineState:state];
        currentPipeline = mtlPso;
    }

    void MetalComputeEncoder::BindResourceGroup(uint32_t set, ResourceGroup *group, uint32_t numDynamicOffsets, const uint32_t *dynamicOffsets)
    {
        (void)set; // see MetalGraphicsEncoder::BindResourceGroup
        if (group == nullptr) {
            return;
        }
        static_cast<MetalResourceGroup *>(group)->BindCompute(computeEncoder, numDynamicOffsets, dynamicOffsets);
    }

    void MetalComputeEncoder::BindDescriptorHeap(DescriptorHeap * /*heap*/)
    {
        // TODO: Metal argument buffer heap bind (aurora-resource-group tier2)
    }

    void MetalComputeEncoder::PushConstants(uint32_t offset, uint32_t size, const void *data)
    {
        // see MetalGraphicsEncoder::PushConstants for the slot/staging convention
        if (data == nullptr || size == 0 || currentPipeline == nullptr) {
            return;
        }
        auto *shader = currentPipeline->GetShader();
        if (shader == nullptr) {
            return;
        }

        uint32_t    length = 0;
        const void *block  = AccumulatePushConstant(pushConstantBlock, shader->GetPushConstantSize(), offset, size, data, length);

        id<MTLComputeCommandEncoder> enc = (__bridge id<MTLComputeCommandEncoder>)computeEncoder;
        [enc setBytes:block length:length atIndex:shader->GetPushConstantSlot()];
    }

    void MetalComputeEncoder::Dispatch(uint32_t groupX, uint32_t groupY, uint32_t groupZ)
    {
        id<MTLComputeCommandEncoder> enc   = (__bridge id<MTLComputeCommandEncoder>)computeEncoder;
        uint32_t                     tg[3] = {1, 1, 1};
        if (currentPipeline != nullptr) {
            currentPipeline->GetThreadGroupSize(tg);
        }
        MTLSize threadsPerGroup = MTLSizeMake(tg[0], tg[1], tg[2]);
        MTLSize threadgroups    = MTLSizeMake(groupX, groupY, groupZ);
        [enc dispatchThreadgroups:threadgroups threadsPerThreadgroup:threadsPerGroup];
    }

    void MetalComputeEncoder::DispatchIndirect(Buffer *buffer, uint64_t offset)
    {
        id<MTLComputeCommandEncoder> enc         = (__bridge id<MTLComputeCommandEncoder>)computeEncoder;
        id<MTLBuffer>                indirectBuf = (__bridge id<MTLBuffer>)static_cast<MetalBuffer *>(buffer)->GetNativeHandle();
        uint32_t                     tg[3]       = {1, 1, 1};
        if (currentPipeline != nullptr) {
            currentPipeline->GetThreadGroupSize(tg);
        }
        MTLSize threadsPerGroup = MTLSizeMake(tg[0], tg[1], tg[2]);
        [enc dispatchThreadgroupsWithIndirectBuffer:indirectBuf indirectBufferOffset:(NSUInteger)offset threadsPerThreadgroup:threadsPerGroup];
    }

    // ---- MetalBlitEncoder ----

    MetalBlitEncoder::MetalBlitEncoder(MetalDevice &device, MetalCommandBuffer *o) : device(device), owner(o)
    {
        id<MTLCommandBuffer>      cb        = (__bridge id<MTLCommandBuffer>)owner->GetNativeHandle();
        id<MTLBlitCommandEncoder> nativeEnc = [cb blitCommandEncoder];
        blitEncoder                         = (__bridge_retained void *)nativeEnc;
        owner->NotifyEncoderBegin(MetalCommandBuffer::ActiveEncoderKind::Blit, (__bridge void *)nativeEnc);
    }

    MetalBlitEncoder::~MetalBlitEncoder()
    {
        if (blitEncoder != nullptr) {
            id<MTLBlitCommandEncoder> enc = (__bridge_transfer id<MTLBlitCommandEncoder>)blitEncoder;
            [enc endEncoding];
            blitEncoder = nullptr;
            owner->NotifyEncoderEnd();
        }
    }

    void MetalBlitEncoder::CopyBuffer(Buffer *src, Buffer *dst, uint64_t size, uint64_t srcOffset, uint64_t dstOffset)
    {
        id<MTLBlitCommandEncoder> enc    = (__bridge id<MTLBlitCommandEncoder>)blitEncoder;
        id<MTLBuffer>             srcBuf = (__bridge id<MTLBuffer>)static_cast<MetalBuffer *>(src)->GetNativeHandle();
        id<MTLBuffer>             dstBuf = (__bridge id<MTLBuffer>)static_cast<MetalBuffer *>(dst)->GetNativeHandle();
        [enc copyFromBuffer:srcBuf sourceOffset:(NSUInteger)srcOffset toBuffer:dstBuf destinationOffset:(NSUInteger)dstOffset size:(NSUInteger)size];
    }

    void MetalBlitEncoder::CopyBufferToImage(Buffer *src, Image *dst, const std::vector<BufferImageCopy> &regions)
    {
        id<MTLBlitCommandEncoder> enc    = (__bridge id<MTLBlitCommandEncoder>)blitEncoder;
        id<MTLBuffer>             srcBuf = (__bridge id<MTLBuffer>)static_cast<MetalBuffer *>(src)->GetNativeHandle();
        id<MTLTexture>            dstTex = (__bridge id<MTLTexture>)static_cast<MetalImage *>(dst)->GetNativeHandle();

        for (const auto &region : regions) {
            MTLSize   sourceSize = MTLSizeMake(region.imageExtent.width, region.imageExtent.height, region.imageExtent.depth);
            MTLOrigin dstOrigin  = MTLOriginMake(region.imageOffset.x, region.imageOffset.y, region.imageOffset.z);

            const uint32_t   rowLength     = region.bufferRowLength > 0 ? region.bufferRowLength : region.imageExtent.width;
            const uint32_t   imageHeight   = region.bufferImageHeight > 0 ? region.bufferImageHeight : region.imageExtent.height;
            const auto      *metalDst      = static_cast<MetalImage *>(dst);
            const NSUInteger bytesPerRow   = static_cast<NSUInteger>(GetImageRowPitch(metalDst->GetPixelFormat(), rowLength));
            const NSUInteger bytesPerImage = static_cast<NSUInteger>(GetImageSlicePitch(metalDst->GetPixelFormat(), rowLength, imageHeight));

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
        id<MTLBlitCommandEncoder> enc    = (__bridge id<MTLBlitCommandEncoder>)blitEncoder;
        id<MTLTexture>            srcTex = (__bridge id<MTLTexture>)static_cast<MetalImage *>(src)->GetNativeHandle();
        id<MTLBuffer>             dstBuf = (__bridge id<MTLBuffer>)static_cast<MetalBuffer *>(dst)->GetNativeHandle();

        for (const auto &region : regions) {
            MTLSize   sourceSize = MTLSizeMake(region.imageExtent.width, region.imageExtent.height, region.imageExtent.depth);
            MTLOrigin srcOrigin  = MTLOriginMake(region.imageOffset.x, region.imageOffset.y, region.imageOffset.z);

            const uint32_t   rowLength     = region.bufferRowLength > 0 ? region.bufferRowLength : region.imageExtent.width;
            const uint32_t   imageHeight   = region.bufferImageHeight > 0 ? region.bufferImageHeight : region.imageExtent.height;
            const auto      *metalSrc      = static_cast<MetalImage *>(src);
            const NSUInteger bytesPerRow   = static_cast<NSUInteger>(GetImageRowPitch(metalSrc->GetPixelFormat(), rowLength));
            const NSUInteger bytesPerImage = static_cast<NSUInteger>(GetImageSlicePitch(metalSrc->GetPixelFormat(), rowLength, imageHeight));

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

    void MetalBlitEncoder::SuspendBlit()
    {
        if (blitEncoder != nullptr) {
            id<MTLBlitCommandEncoder> enc = (__bridge_transfer id<MTLBlitCommandEncoder>)blitEncoder;
            [enc endEncoding];
            blitEncoder = nullptr;
            owner->NotifyEncoderEnd();
        }
    }

    void MetalBlitEncoder::ResumeBlit()
    {
        if (blitEncoder == nullptr) {
            id<MTLCommandBuffer>      cb        = (__bridge id<MTLCommandBuffer>)owner->GetNativeHandle();
            id<MTLBlitCommandEncoder> nativeEnc = [cb blitCommandEncoder];
            blitEncoder                         = (__bridge_retained void *)nativeEnc;
            owner->NotifyEncoderBegin(MetalCommandBuffer::ActiveEncoderKind::Blit, (__bridge void *)nativeEnc);
        }
    }

    void MetalBlitEncoder::BlitImage(Image *src, Image *dst, const std::vector<BlitInfo> &regions, Filter filter)
    {
        auto          *srcImage = static_cast<MetalImage *>(src);
        auto          *dstImage = static_cast<MetalImage *>(dst);
        id<MTLTexture> srcTex   = (__bridge id<MTLTexture>)srcImage->GetNativeHandle();
        id<MTLTexture> dstTex   = (__bridge id<MTLTexture>)dstImage->GetNativeHandle();

        // fast path: 1:1 texel copies (same format, same extent, no filtering) map
        // to native blit-encoder copies
        bool allCopies = srcTex.pixelFormat == dstTex.pixelFormat;
        if (allCopies) {
            for (const auto &region : regions) {
                const auto srcW = region.srcOffsets[1].x - region.srcOffsets[0].x;
                const auto srcH = region.srcOffsets[1].y - region.srcOffsets[0].y;
                const auto srcD = region.srcOffsets[1].z - region.srcOffsets[0].z;
                const auto dstW = region.dstOffsets[1].x - region.dstOffsets[0].x;
                const auto dstH = region.dstOffsets[1].y - region.dstOffsets[0].y;
                const auto dstD = region.dstOffsets[1].z - region.dstOffsets[0].z;
                if (srcW != dstW || srcH != dstH || srcD != dstD) {
                    allCopies = false;
                    break;
                }
            }
        }

        if (allCopies) {
            id<MTLBlitCommandEncoder> enc = (__bridge id<MTLBlitCommandEncoder>)blitEncoder;
            for (const auto &region : regions) {
                const uint32_t layers = std::max(region.dstRange.layers, 1u);
                for (uint32_t i = 0; i < layers; ++i) {
                    MTLSize   size = MTLSizeMake(region.srcOffsets[1].x - region.srcOffsets[0].x, region.srcOffsets[1].y - region.srcOffsets[0].y,
                                                 region.srcOffsets[1].z - region.srcOffsets[0].z);
                    MTLOrigin srcOrigin = MTLOriginMake(region.srcOffsets[0].x, region.srcOffsets[0].y, region.srcOffsets[0].z);
                    MTLOrigin dstOrigin = MTLOriginMake(region.dstOffsets[0].x, region.dstOffsets[0].y, region.dstOffsets[0].z);
                    [enc copyFromTexture:srcTex
                              sourceSlice:region.srcRange.baseLayer + (region.srcRange.layers > 1 ? i : 0)
                              sourceLevel:region.srcRange.level
                             sourceOrigin:srcOrigin
                               sourceSize:size
                                toTexture:dstTex
                         destinationSlice:region.dstRange.baseLayer + i
                         destinationLevel:region.dstRange.level
                        destinationOrigin:dstOrigin];
                }
            }
            return;
        }

        // scaling / filtering needs a render pass; suspend the blit encoder around it
        SuspendBlit();
        const bool ok = device.GetBlitHelper()->Blit(owner->GetNativeHandle(), srcImage, dstImage, regions, filter);
        if (!ok) {
            LOG_E(TAG, "BlitImage failed (dst format not renderable?)");
        }
        ResumeBlit();
    }

    void MetalBlitEncoder::ResolveImage(Image *src, Image *dst, const std::vector<ResolveInfo> &regions)
    {
        // Metal resolves via a store-action resolve render pass, not the blit encoder
        SuspendBlit();
        device.GetBlitHelper()->Resolve(owner->GetNativeHandle(), static_cast<MetalImage *>(src), static_cast<MetalImage *>(dst), regions);
        ResumeBlit();
    }

} // namespace sky::aurora
