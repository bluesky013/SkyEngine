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
        void CmdDrawIndirect(const BufferPtr &buffer, uint32_t offset, uint32_t size);
        void CmdEndPass();

        void Attach(Queue &queue);

        static constexpr uint32_t MAX_SET_ID = 4;

    private:
        void BeginPassInternal();

        Context *context = nullptr;
        PipelineCacheState *cache = nullptr;
        rhi::ClearValue *clearValues = nullptr;
        uint32_t clearCount = 0;

        GraphicsPipelinePtr currentPso;
        FrameBufferPtr currentFramebuffer;
        uint32_t currentSubPassId = 0;
        std::vector<DescriptorSetPtr> sets;
    };
}