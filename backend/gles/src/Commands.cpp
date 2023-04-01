//
// Created by Zach Lee on 2023/2/5.
//

#include <gles/CommandContext.h>
#include <gles/Core.h>
#include <gles/Queue.h>

namespace sky::gles {

    void CommandContext::CmdBeginPass(const FrameBufferPtr &frameBuffer, const RenderPassPtr &renderPass, uint32_t clearCount, rhi::ClearValue *values)
    {
        auto  glesFb = std::static_pointer_cast<FrameBuffer>(frameBuffer);
        auto &fbs    = glesFb->GetNativeHandles();
        auto fb0   = fbs[0];
        auto clear = values[0];

        if (fb0.surface) {
            if (fb0.surface->GetSurface() != context->GetCurrentSurface()) {
                context->MakeCurrent(*fb0.surface);
            }
        }
        glBindFramebuffer(GL_FRAMEBUFFER, fb0.fbo);
        glClear(GL_COLOR_BUFFER_BIT);
        glClearColor(clear.color.float32[0], clear.color.float32[1], clear.color.float32[2], clear.color.float32[3]);

        auto &ext = glesFb->GetExtent();
        glViewport(0, 0, ext.width, ext.height);
        glScissor(0, 0, ext.width, ext.height);

        cache->drawBuffer = fb0.fbo;
    }

    void CommandContext::CmdBindDescriptorSet(uint32_t setId, const DescriptorSetPtr &set)
    {

    }

    void CommandContext::CmdBindPipeline(const GraphicsPipelinePtr &pso)
    {
        auto program = pso->GetProgram();
        if (cache->program != program) {
            cache->program = program;
            glUseProgram(program);
        }
    }

    void CommandContext::CmdBindAssembly(const VertexAssemblyPtr &assembly)
    {
        auto vao = assembly->GetNativeHandle();
        if (cache->vao != vao) {
            cache->vao = vao;
            glBindVertexArray(vao);
        }
    }

    void CommandContext::CmdSetViewport(uint32_t count, const rhi::Viewport *viewport)
    {
        glViewport(static_cast<GLint>(viewport->x), static_cast<GLint>(viewport->y),
                   static_cast<GLsizei>(viewport->width), static_cast<GLsizei>(viewport->height));

        glDepthRangef(viewport->minDepth, viewport->maxDepth);
    }

    void CommandContext::CmdSetScissor(uint32_t count, const rhi::Rect2D *scissor)
    {
        glScissor(scissor->offset.x, scissor->offset.y, scissor->extent.width, scissor->extent.height);
    }

    void CommandContext::CmdDrawIndexed(const rhi::CmdDrawIndexed &indexed)
    {
//        glDrawElementsInstanced();
    }

    void CommandContext::CmdDrawLinear(const rhi::CmdDrawLinear &linear)
    {
        CHECK(glDrawArraysInstanced(GL_TRIANGLES, linear.firstVertex, linear.vertexCount, linear.instanceCount));
    }

    void CommandContext::CmdDrawIndirect(const BufferPtr &buffer, uint32_t offset, uint32_t size)
    {
    }

    void CommandContext::CmdEndPass()
    {
    }

    void CommandContext::Attach(Queue &queue)
    {
        context = queue.GetContext();
        cache = queue.GetCacheState();
    }
}