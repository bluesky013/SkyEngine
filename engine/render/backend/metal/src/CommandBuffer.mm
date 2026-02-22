//
// Created by Zach Lee on 2023/5/28.
//

#include <mtl/CommandBuffer.h>
#include <mtl/Device.h>
#include <mtl/Fence.h>
#include <mtl/Semaphore.h>
#include <mtl/Buffer.h>
#include <mtl/VertexAssembly.h>

namespace sky::mtl {
    CommandBuffer::~CommandBuffer()
    {
    }

    bool CommandBuffer::Init(const Descriptor &desc)
    {
        auto *mtlQueue = static_cast<Queue*>(device.GetQueue(desc.queueType));
        queue = mtlQueue->GetNativeHandle();
        return true;
    }

    void CommandBuffer::Begin()
    {
        releasePool = [[NSAutoreleasePool alloc] init];
        currentCommandBuffer = [queue commandBuffer];
    }

    void CommandBuffer::End()
    {
    }

    void CommandBuffer::Submit(rhi::Queue &queue, const rhi::SubmitInfo &submit)
    {
        for (auto &[stage, sem] : submit.waits) {
            std::static_pointer_cast<Semaphore>(sem)->Wait(currentCommandBuffer);
        }
        for (auto &signal : submit.submitSignals) {
            std::static_pointer_cast<Semaphore>(signal)->Signal(currentCommandBuffer);
        }
        if (submit.fence) {
            std::static_pointer_cast<Fence>(submit.fence)->Signal(currentCommandBuffer);
        }
        [currentCommandBuffer commit];

        [releasePool release];
        releasePool = nil;
    }

    void CommandBuffer::ResetQueryPool(const rhi::QueryPoolPtr &queryPool, uint32_t first, uint32_t count)
    {
    }

    void CommandBuffer::GetQueryResult(const rhi::QueryPoolPtr &queryPool, uint32_t first, uint32_t count,
                        const rhi::BufferPtr &result, uint32_t offset, uint32_t stride)
    {
        const auto &mtlPool = std::static_pointer_cast<QueryPool>(queryPool);
        uint32_t queryFirst = mtlPool->GetQueryType() == rhi::QueryType::TIME_STAMP ? first : first * 2;
        uint32_t queryCount = mtlPool->GetQueryType() == rhi::QueryType::TIME_STAMP ? count : count * 2;

        auto encoder = [currentCommandBuffer blitCommandEncoder];
        [encoder resolveCounters: mtlPool->GetCounterSamplerBuffer()
                         inRange: NSMakeRange(queryFirst, queryCount)
               destinationBuffer: std::static_pointer_cast<Buffer>(result)->GetNativeHandle()
               destinationOffset: offset];
        [encoder endEncoding];
    };

    rhi::GraphicsEncoder &GraphicsEncoder::BeginQuery(const rhi::QueryPoolPtr &query, uint32_t id)
    {
        const auto &mtlQuery = std::static_pointer_cast<QueryPool>(query);
        [encoder sampleCountersInBuffer: mtlQuery->GetCounterSamplerBuffer()
                          atSampleIndex: id * 2
                            withBarrier: NO];
        return *this;
    }

    rhi::GraphicsEncoder &GraphicsEncoder::EndQuery(const rhi::QueryPoolPtr &query, uint32_t id)
    {
        const auto &mtlQuery = std::static_pointer_cast<QueryPool>(query);
        [encoder sampleCountersInBuffer: mtlQuery->GetCounterSamplerBuffer()
                          atSampleIndex: id * 2 + 1
                            withBarrier: NO];
        return *this;
    }

    rhi::GraphicsEncoder &GraphicsEncoder::WriteTimeStamp(const rhi::QueryPoolPtr &query, rhi::PipelineStageBit stage, uint32_t id)
    {
        const auto &mtlQuery = std::static_pointer_cast<QueryPool>(query);
        [encoder sampleCountersInBuffer: mtlQuery->GetCounterSamplerBuffer()
                          atSampleIndex: id
                            withBarrier: NO];
        return *this;
    };

    rhi::GraphicsEncoder &GraphicsEncoder::BeginPass(const rhi::PassBeginInfo &beginInfo)
    {
        currentRenderPass = std::static_pointer_cast<RenderPass>(beginInfo.renderPass);
        currentFramebuffer = std::static_pointer_cast<FrameBuffer>(beginInfo.frameBuffer);
        passDesc = currentFramebuffer->RequestRenderPassDescriptor(currentRenderPass, beginInfo.clearCount, beginInfo.clearValues);
        encoder = [commandBuffer.GetNativeHandle() renderCommandEncoderWithDescriptor: passDesc];
        currentSubpass = 0;
        return *this;
    }

    rhi::GraphicsEncoder &GraphicsEncoder::BindPipeline(const rhi::GraphicsPipelinePtr &pso)
    {
        auto pipeline = std::static_pointer_cast<GraphicsPipeline>(pso);
        auto &rasterizerState = pipeline->GetRasterizerState();

        // pipeline state
        primitive = pipeline->GetPrimitiveType();
        [encoder setRenderPipelineState: pipeline->GetRenderPipelineState()];
        [encoder setFrontFacingWinding: rasterizerState.frontFace];
        [encoder setCullMode: rasterizerState.cullMode];
        [encoder setTriangleFillMode: rasterizerState.fillMode];
        [encoder setDepthClipMode: rasterizerState.depthClipMode];
        [encoder setDepthBias: rasterizerState.depthBias
                   slopeScale: rasterizerState.depthSlopeScale
                        clamp: rasterizerState.depthBiasClamp];

        // depth stencil
        [encoder setDepthStencilState: pipeline->GetDepthStencilState()];
        [encoder setStencilFrontReferenceValue: pipeline->GetFrontReference()
                            backReferenceValue: pipeline->GetBackReference()];
        return *this;
    }

    rhi::GraphicsEncoder &GraphicsEncoder::BindAssembly(const rhi::VertexAssemblyPtr &assembly)
    {
        currentVa = std::static_pointer_cast<VertexAssembly>(assembly);
        currentVa->OnBind(encoder);
        return *this;
    }

    rhi::GraphicsEncoder &GraphicsEncoder::BindVertexBuffers(const std::vector<rhi::BufferView> &vbs)
    {
        size_t bufferNum = vbs.size();
        std::vector<id<MTLBuffer>> buffers(bufferNum);
        std::vector<NSUInteger> offsets(bufferNum);

        for (uint32_t i = 0; i < vbs.size(); ++i) {
            buffers[i] = static_cast<Buffer*>(vbs[i].buffer.get())->GetNativeHandle();
            offsets[i] = vbs[i].offset;
        }
        NSRange range = {0, static_cast<NSUInteger>(bufferNum)};

        [encoder setVertexBuffers: buffers.data()
                          offsets: offsets.data()
                        withRange: range];
        return *this;
    }

    rhi::GraphicsEncoder &GraphicsEncoder::BindIndexBuffer(const rhi::BufferView& view, rhi::IndexType type)
    {
        indexBuffer = static_cast<Buffer*>(view.buffer.get())->GetNativeHandle();
        indexOffset = view.offset;
        indexType = type == rhi::IndexType::U32 ? MTLIndexTypeUInt32 : MTLIndexTypeUInt16;
        return *this;
    }

    rhi::GraphicsEncoder &GraphicsEncoder::SetViewport(uint32_t count, const rhi::Viewport *viewports)
    {
        MTLViewport mtlViewport = {};
        auto       &viewport    = viewports[0];
        mtlViewport.width       = viewport.width;
        mtlViewport.height      = viewport.height;
        mtlViewport.originX     = viewport.x;
        mtlViewport.originY     = viewport.y;
        mtlViewport.znear       = viewport.minDepth;
        mtlViewport.zfar        = viewport.maxDepth;

        [encoder setViewport: mtlViewport];
        return *this;
    }

    rhi::GraphicsEncoder &GraphicsEncoder::SetScissor(uint32_t count, const rhi::Rect2D *scissors)
    {
        MTLScissorRect mtlScissor = {};
        auto &scissor = scissors[0];
        mtlScissor.x = scissor.offset.x;
        mtlScissor.y = scissor.offset.y;
        mtlScissor.width = scissor.extent.width;
        mtlScissor.height = scissor.extent.height;

        [encoder setScissorRect: mtlScissor];
        return *this;
    }

    rhi::GraphicsEncoder &GraphicsEncoder::DrawIndexed(const rhi::CmdDrawIndexed &indexed)
    {
        SKY_ASSERT(indexBuffer != nil);
        NSUInteger offset = indexed.firstIndex * (indexType == MTLIndexTypeUInt32 ? 4 : 2) + indexOffset;
        [encoder drawIndexedPrimitives: primitive
                            indexCount: indexed.indexCount
                             indexType: indexType
                           indexBuffer: indexBuffer
                     indexBufferOffset: offset
                         instanceCount: indexed.instanceCount
                            baseVertex: indexed.vertexOffset
                          baseInstance: indexed.firstInstance];
        return *this;
    }

    rhi::GraphicsEncoder &GraphicsEncoder::DrawLinear(const rhi::CmdDrawLinear &linear)
    {
        [encoder drawPrimitives: primitive
                    vertexStart: linear.firstVertex
                    vertexCount: linear.vertexCount
                  instanceCount: linear.instanceCount];
        return *this;
    }

    rhi::GraphicsEncoder &GraphicsEncoder::DrawIndexedIndirect(const rhi::BufferPtr &buffer,
                                                               uint32_t offset, uint32_t count, uint32_t stride)
    {
        auto mtlBuffer = std::static_pointer_cast<Buffer>(buffer);
        for (uint32_t i = 0; i < count; ++i) {
            const auto off = offset + i * stride;
            [encoder drawIndexedPrimitives: primitive
                                 indexType: indexType
                               indexBuffer: indexBuffer
                         indexBufferOffset: offset
                            indirectBuffer: mtlBuffer->GetNativeHandle()
                      indirectBufferOffset: off];
        };
        return *this;
    }

    rhi::GraphicsEncoder &GraphicsEncoder::DrawIndirect(const rhi::BufferPtr &buffer,
                                                        uint32_t offset, uint32_t count, uint32_t stride)
    {
        auto mtlBuffer = std::static_pointer_cast<Buffer>(buffer);
        for (uint32_t i = 0; i < count; ++i) {
            const auto off = offset + i * stride;
            [encoder drawPrimitives: primitive
                      indirectBuffer: mtlBuffer->GetNativeHandle()
                indirectBufferOffset: off];
        };
        return *this;
    }

    rhi::GraphicsEncoder &GraphicsEncoder::DispatchMesh(const rhi::CmdDispatchMesh &dispatch)
    {
        SKY_ASSERT(false && "not implement");
        return *this;
    }

    rhi::GraphicsEncoder &GraphicsEncoder::BindSet(uint32_t id, const rhi::DescriptorSetPtr &set)
    {
        return *this;
    }

    rhi::GraphicsEncoder &GraphicsEncoder::NextSubPass()
    {
        ++currentSubpass;
        return *this;
    }

    rhi::GraphicsEncoder &GraphicsEncoder::EndPass()
    {
        [encoder endEncoding];
        return *this;
    }

    rhi::GraphicsEncoder &GraphicsEncoder::SetOffset(uint32_t set, uint32_t binding, uint32_t index, uint32_t offset)
    {
        return *this;
    }

    BlitEncoder::BlitEncoder(CommandBuffer &cmd) : commandBuffer(cmd)
    {
        encoder = [commandBuffer.GetNativeHandle() blitCommandEncoder];
    }

    rhi::BlitEncoder &BlitEncoder::CopyTexture()
    {
        return *this;
    }

    rhi::BlitEncoder &BlitEncoder::CopyTextureToBuffer()
    {
        return *this;
    }

    rhi::BlitEncoder &BlitEncoder::CopyBufferToTexture()
    {
        return *this;
    }

    rhi::BlitEncoder &BlitEncoder::BlitTexture(const rhi::ImagePtr &src, const rhi::ImagePtr &dst,
                                  const std::vector<rhi::BlitInfo> &blitInputs, rhi::Filter filter)
    {
        return *this;
    }

    rhi::BlitEncoder &BlitEncoder::ResoleTexture(const rhi::ImagePtr &src, const rhi::ImagePtr &dst,
                                    const std::vector<rhi::ResolveInfo> &resolveInputs)
    {
        return *this;
    }

    rhi::BlitEncoder &BlitEncoder::CopyBuffer(const rhi::BufferPtr &src, const rhi::BufferPtr &dst,
                                 uint64_t size, uint64_t srcOffset, uint64_t dstOffset)
    {
        return *this;
    }
} // namespace sky::mtl
