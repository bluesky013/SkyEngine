//
// Created by Zach on 2023/1/31.
//

#include <gles/CommandBuffer.h>

namespace sky::gles {
    void GraphicsEncoder::BeginPass(const rhi::FrameBufferPtr &frameBuffer, const rhi::RenderPassPtr &renderPass, uint32_t clearCount, rhi::ClearValue *clearValues)
    {
        uint32_t copySize = clearCount * sizeof(rhi::ClearValue);
        rhi::ClearValue *copyValues = reinterpret_cast<rhi::ClearValue*>(commandBuffer.Allocate(copySize));
        memcpy(copyValues, clearValues, copySize);
        commandBuffer.EnqueueMessage([](const rhi::FrameBufferPtr &fb, const rhi::RenderPassPtr &pass, uint32_t count, rhi::ClearValue *values) {


        }, frameBuffer, renderPass, clearCount, copyValues);
    }

    void GraphicsEncoder::BindPipeline(const rhi::GraphicsPipelinePtr &pso)
    {
        commandBuffer.EnqueueMessage([](const rhi::GraphicsPipelinePtr &pso) {
        }, pso);
    }

    void GraphicsEncoder::BindAssembly(const rhi::VertexAssemblyPtr &assembly)
    {
        commandBuffer.EnqueueMessage([](const rhi::VertexAssemblyPtr &assembly) {
        }, assembly);
    }

    void GraphicsEncoder::SetViewport(uint32_t count, const rhi::Viewport *viewport)
    {
        uint32_t copySize = count * sizeof(rhi::Viewport);
        rhi::Viewport *copyValues = reinterpret_cast<rhi::Viewport*>(commandBuffer.Allocate(copySize));
        memcpy(copyValues, viewport, copySize);
        commandBuffer.EnqueueMessage([](uint32_t count, const rhi::Viewport *viewport) {
        }, count, copyValues);
    }

    void GraphicsEncoder::SetScissor(uint32_t count, const rhi::Rect2D *scissor)
    {
        uint32_t copySize = count * sizeof(rhi::Rect2D);
        rhi::Rect2D *copyValues = reinterpret_cast<rhi::Rect2D*>(commandBuffer.Allocate(copySize));
        memcpy(copyValues, scissor, copySize);
        commandBuffer.EnqueueMessage([](uint32_t count, const rhi::Rect2D *scissor) {
        }, count, scissor);
    }

    void GraphicsEncoder::DrawIndexed(const rhi::CmdDrawIndexed &indexed)
    {
        commandBuffer.EnqueueMessage([](const rhi::CmdDrawIndexed &indexed) {
        }, indexed);
    }

    void GraphicsEncoder::DrawLinear(const rhi::CmdDrawLinear &linear)
    {
        commandBuffer.EnqueueMessage([](const rhi::CmdDrawLinear &linear) {
        }, linear);
    }

    void GraphicsEncoder::DrawIndirect(const rhi::BufferPtr &buffer, uint32_t offset, uint32_t size)
    {
        commandBuffer.EnqueueMessage([](const rhi::BufferPtr &buffer, uint32_t offset, uint32_t size) {
        }, buffer, offset, size);
    }

    void GraphicsEncoder::EndPass()
    {
    }

    void CommandBuffer::Reset()
    {
        for (auto &storage : storages) {
            storage->Reset();
        }
        iterator = storages.begin();
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

    std::shared_ptr<rhi::GraphicsEncoder> CommandBuffer::EncodeGraphics()
    {
        return std::static_pointer_cast<rhi::GraphicsEncoder>(std::make_shared<GraphicsEncoder>(*this));
    }
}
