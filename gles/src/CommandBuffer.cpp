//
// Created by Zach on 2023/1/31.
//

#include <gles/CommandBuffer.h>
#include <gles/Queue.h>
#include <gles/FrameBuffer.h>
#include <gles/Device.h>
#include <gles/Core.h>

namespace sky::gles {
    rhi::GraphicsEncoder &GraphicsEncoder::BeginPass(const rhi::FrameBufferPtr &frameBuffer, const rhi::RenderPassPtr &renderPass, uint32_t clearCount, rhi::ClearValue *clearValues)
    {
        uint32_t copySize = clearCount * sizeof(rhi::ClearValue);
        rhi::ClearValue *copyValues = reinterpret_cast<rhi::ClearValue*>(commandBuffer.Allocate(copySize));
        memcpy(copyValues, clearValues, copySize);
        commandBuffer.EnqueueMessage([](const rhi::FrameBufferPtr &fb, const rhi::RenderPassPtr &pass, uint32_t count, rhi::ClearValue *values) {
            auto glesFb = std::static_pointer_cast<FrameBuffer>(fb);
            auto &fbs = glesFb->GetNativeHandles();

            auto fb0 = fbs[0];
            auto clear = values[0];

            if (fb0.surface) {
                auto surface = fb0.surface->GetSurface();
                EGLSurface current = eglGetCurrentSurface(EGL_DRAW);
                EGLContext context = eglGetCurrentContext();
                if (surface != current) {
                    eglMakeCurrent(eglGetDisplay(EGL_DEFAULT_DISPLAY), surface, surface, context);
                }
            }

            glBindFramebuffer(GL_FRAMEBUFFER, fb0.fbo);
            glClear(GL_COLOR_BUFFER_BIT);
            glClearColor(clear.color.float32[0], clear.color.float32[1], clear.color.float32[2], clear.color.float32[3]);

            glBindFramebuffer(GL_FRAMEBUFFER, 0);

            if (fb0.surface) {
                eglSwapBuffers(eglGetDisplay(EGL_DEFAULT_DISPLAY), fb0.surface->GetSurface());
            }
            CHECK();

        }, frameBuffer, renderPass, clearCount, copyValues);
        return *this;
    }

    rhi::GraphicsEncoder &GraphicsEncoder::BindPipeline(const rhi::GraphicsPipelinePtr &pso)
    {
        commandBuffer.EnqueueMessage([](const rhi::GraphicsPipelinePtr &pso) {
        }, pso);
        return *this;
    }

    rhi::GraphicsEncoder &GraphicsEncoder::BindAssembly(const rhi::VertexAssemblyPtr &assembly)
    {
        commandBuffer.EnqueueMessage([](const rhi::VertexAssemblyPtr &assembly) {
        }, assembly);
        return *this;
    }

    rhi::GraphicsEncoder &GraphicsEncoder::SetViewport(uint32_t count, const rhi::Viewport *viewport)
    {
        uint32_t copySize = count * sizeof(rhi::Viewport);
        rhi::Viewport *copyValues = reinterpret_cast<rhi::Viewport*>(commandBuffer.Allocate(copySize));
        memcpy(copyValues, viewport, copySize);
        commandBuffer.EnqueueMessage([](uint32_t count, const rhi::Viewport *viewport) {
        }, count, copyValues);
        return *this;
    }

    rhi::GraphicsEncoder &GraphicsEncoder::SetScissor(uint32_t count, const rhi::Rect2D *scissor)
    {
        uint32_t copySize = count * sizeof(rhi::Rect2D);
        rhi::Rect2D *copyValues = reinterpret_cast<rhi::Rect2D*>(commandBuffer.Allocate(copySize));
        memcpy(copyValues, scissor, copySize);
        commandBuffer.EnqueueMessage([](uint32_t count, const rhi::Rect2D *scissor) {
        }, count, scissor);
        return *this;
    }

    rhi::GraphicsEncoder &GraphicsEncoder::DrawIndexed(const rhi::CmdDrawIndexed &indexed)
    {
        commandBuffer.EnqueueMessage([](const rhi::CmdDrawIndexed &indexed) {
        }, indexed);
        return *this;
    }

    rhi::GraphicsEncoder &GraphicsEncoder::DrawLinear(const rhi::CmdDrawLinear &linear)
    {
        commandBuffer.EnqueueMessage([](const rhi::CmdDrawLinear &linear) {
        }, linear);
        return *this;
    }

    rhi::GraphicsEncoder &GraphicsEncoder::DrawIndirect(const rhi::BufferPtr &buffer, uint32_t offset, uint32_t size)
    {
        commandBuffer.EnqueueMessage([](const rhi::BufferPtr &buffer, uint32_t offset, uint32_t size) {
        }, buffer, offset, size);
        return *this;
    }

    rhi::GraphicsEncoder &GraphicsEncoder::EndPass()
    {
        return *this;
    }

    void CommandBuffer::Reset()
    {
        for (auto &storage : storages) {
            storage->Reset();
        }
        iterator = storages.begin();
        head = nullptr;
        current = &head;
    }

    uint8_t* CommandBuffer::Allocate(uint32_t size)
    {
        uint8_t *ptr = (*iterator)->Allocate(size);
        if (ptr == nullptr) {
            iterator++;
            if (iterator == storages.end()) {
                AllocateStorage();
            }
            ptr = (*iterator)->Allocate(size);
        }
        return ptr;
    }

    void CommandBuffer::AllocateStorage()
    {
        static const uint32_t DEFAULT_SIZE = 8 * 1024 * 1024; // 8M
        auto storage = std::make_unique<CommandStorage>();
        storage->Init({DEFAULT_SIZE});
        iterator = storages.emplace(iterator, std::move(storage));
    }

    bool CommandBuffer::Init(const Descriptor &desc)
    {
        AllocateStorage();
        fence = std::static_pointer_cast<Fence>(device.CreateFence({}));
        return true;
    }

    void CommandBuffer::Execute()
    {
        while (head != nullptr) {
            auto ptr = head->next;
            head->Execute();
            head->~TaskBase();
            head = ptr;
        }
    }

    void CommandBuffer::Begin()
    {
        fence->Wait();
        fence->Reset();
        Reset();
    }

    void CommandBuffer::End()
    {
    }

    void CommandBuffer::Submit(rhi::Queue &queue)
    {
        auto &glesQueue = static_cast<Queue&>(queue);
        context = glesQueue.GetContext();
        glesQueue.CreateTask([this]() {
            Execute();
            context = nullptr;
            fence->Signal();
        });
    }

    std::shared_ptr<rhi::GraphicsEncoder> CommandBuffer::EncodeGraphics()
    {
        return std::static_pointer_cast<rhi::GraphicsEncoder>(std::make_shared<GraphicsEncoder>(*this));
    }
}
