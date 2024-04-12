//
// Created by Zach Lee on 2023/2/5.
//

#pragma once

#include <rhi/Core.h>
#include <rhi/Commands.h>
#include <gles/GraphicsPipeline.h>
#include <gles/VertexAssembly.h>
#include <gles/FrameBuffer.h>
#include <gles/Buffer.h>
#include <gles/DescriptorSet.h>
#include <gles/PipelineStateCache.h>
#include <gles/egl/Context.h>
#include <memory>

namespace sky::gles {
    class Queue;

    class CommandContext {
    public:
        CommandContext()  = default;
        ~CommandContext() = default;

        void CmdBeginPass(const FrameBufferPtr &frameBuffer, const RenderPassPtr &renderPass, uint32_t clearCount, rhi::ClearValue *clearValues);
        void CmdNextPass();
        void CmdBindDescriptorSet(uint32_t setId, const DescriptorSetPtr &set, uint32_t offsetCount, uint32_t *offsetValues);
        void CmdBindPipeline(const GraphicsPipelinePtr &pso);
        void CmdBindAssembly(const VertexAssemblyPtr &assembly);
        void CmdSetViewport(uint32_t count, const rhi::Viewport *viewport);
        void CmdSetScissor(uint32_t count, const rhi::Rect2D *scissor);
        void CmdDrawIndexed(const rhi::CmdDrawIndexed &indexed);
        void CmdDrawLinear(const rhi::CmdDrawLinear &linear);
        void CmdDrawIndirect(const BufferPtr &buffer, uint32_t offset, uint32_t count, uint32_t stride);
        void CmdDrawIndexedIndirect(const BufferPtr &buffer, uint32_t offset, uint32_t count, uint32_t stride);
        void CmdEndPass();

        void Attach(Queue &queue);

        static constexpr uint32_t MAX_SET_ID = 4;

    private:
        enum StencilFace : uint8_t {
            FRONT,
            BACK
        };

        void BeginPassInternal();
        void EndPassInternal();
        void SetDepthStencil(const DepthStencilState &ds);
        void SetRasterizerState(const RasterizerState &rs);

        void SetStencilWriteMask(StencilFace face, uint32_t mask);
        void SetStencilFunc(StencilFace face, GLenum func, uint32_t ref, uint32_t mask);
        void SetStencilOp(StencilFace face, GLenum sFail, GLenum dpFail, GLenum dpPass);
        void SetDepthBound(float minBounds, float maxBounds);
        void SetLineWidth(float width);
        void SetDepthBias(float constant, float clamp, float slope);
        void SetBlendState(const BlendState &bs);
        Context *context = nullptr;
        PipelineCacheState *cache = nullptr;
        rhi::QueueType type = rhi::QueueType::GRAPHICS;
        rhi::ClearValue *clearValues = nullptr;
        uint32_t clearCount = 0;

        GraphicsPipelinePtr currentPso;
        FrameBufferPtr currentFramebuffer;
        RenderPassPtr currentRenderPass;
        uint32_t currentSubPassId = 0;
        std::vector<DescriptorSetPtr> sets;
    };
}
