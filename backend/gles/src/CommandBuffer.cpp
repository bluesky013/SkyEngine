//
// Created by Zach on 2023/1/31.
//

#include <gles/CommandBuffer.h>
#include <gles/Queue.h>
#include <gles/FrameBuffer.h>
#include <gles/Device.h>
#include <gles/Core.h>

namespace sky::gles {
    rhi::GraphicsEncoder &GraphicsEncoder::BeginPass(const rhi::PassBeginInfo &beginInfo)
    {
        uint32_t copySize = beginInfo.clearCount * sizeof(rhi::ClearValue);
        rhi::ClearValue *copyValues = reinterpret_cast<rhi::ClearValue*>(commandBuffer.Allocate(copySize));
        memcpy(copyValues, beginInfo.clearValues, copySize);
        commandBuffer.EnqueueMessage(&CommandContext::CmdBeginPass, context,
                                     std::static_pointer_cast<FrameBuffer>(beginInfo.frameBuffer),
                                     std::static_pointer_cast<RenderPass>(beginInfo.renderPass),
                                     beginInfo.clearCount, copyValues);
        return *this;
    }

    rhi::GraphicsEncoder &GraphicsEncoder::BindPipeline(const rhi::GraphicsPipelinePtr &pso)
    {
        commandBuffer.EnqueueMessage(&CommandContext::CmdBindPipeline, context,
                                     std::static_pointer_cast<GraphicsPipeline>(pso));
        return *this;
    }

    rhi::GraphicsEncoder &GraphicsEncoder::BindAssembly(const rhi::VertexAssemblyPtr &assembly)
    {
        commandBuffer.EnqueueMessage(&CommandContext::CmdBindAssembly, context,
                                     std::static_pointer_cast<VertexAssembly>(assembly));
        return *this;
    }

    rhi::GraphicsEncoder &GraphicsEncoder::SetViewport(uint32_t count, const rhi::Viewport *viewport)
    {
        uint32_t copySize = count * sizeof(rhi::Viewport);
        rhi::Viewport *copyValues = reinterpret_cast<rhi::Viewport*>(commandBuffer.Allocate(copySize));
        memcpy(copyValues, viewport, copySize);
        commandBuffer.EnqueueMessage(&CommandContext::CmdSetViewport, context, count, copyValues);
        return *this;
    }

    rhi::GraphicsEncoder &GraphicsEncoder::SetScissor(uint32_t count, const rhi::Rect2D *scissor)
    {
        uint32_t copySize = count * sizeof(rhi::Rect2D);
        rhi::Rect2D *copyValues = reinterpret_cast<rhi::Rect2D*>(commandBuffer.Allocate(copySize));
        memcpy(copyValues, scissor, copySize);
        commandBuffer.EnqueueMessage(&CommandContext::CmdSetScissor, context, count, scissor);
        return *this;
    }

    rhi::GraphicsEncoder &GraphicsEncoder::DrawIndexed(const rhi::CmdDrawIndexed &indexed)
    {
        commandBuffer.EnqueueMessage(&CommandContext::CmdDrawIndexed, context, indexed);
        return *this;
    }

    rhi::GraphicsEncoder &GraphicsEncoder::DrawLinear(const rhi::CmdDrawLinear &linear)
    {
        commandBuffer.EnqueueMessage(&CommandContext::CmdDrawLinear, context, linear);
        return *this;
    }

    rhi::GraphicsEncoder &GraphicsEncoder::DrawIndirect(const rhi::BufferPtr &buffer, uint32_t offset, uint32_t size)
    {
        commandBuffer.EnqueueMessage(&CommandContext::CmdDrawIndirect, context,
                                     std::static_pointer_cast<Buffer>(buffer),
                                     offset, size);
        return *this;
    }

    rhi::GraphicsEncoder &GraphicsEncoder::BindSet(uint32_t id, const rhi::DescriptorSetPtr &set)
    {
        commandBuffer.EnqueueMessage(&CommandContext::CmdBindDescriptorSet, context,
                                     id,
                                     std::static_pointer_cast<DescriptorSet>(set),
                                     0, nullptr);
        return *this;
    }

    rhi::GraphicsEncoder &GraphicsEncoder::EndPass()
    {
        commandBuffer.EnqueueMessage(&CommandContext::CmdEndPass, context);
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
        context = std::make_unique<CommandContext>();
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

    CommandBuffer::~CommandBuffer() noexcept
    {
        fence->Wait();
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

    void CommandBuffer::Submit(rhi::Queue &queue, const rhi::SubmitInfo &info)
    {
        auto &glesQueue = static_cast<Queue&>(queue);
        context->Attach(glesQueue);
        glesQueue.CreateTask([this]() {
            Execute();
            fence->Signal();
        });
    }

    std::shared_ptr<rhi::GraphicsEncoder> CommandBuffer::EncodeGraphics()
    {
        return std::static_pointer_cast<rhi::GraphicsEncoder>(std::make_shared<GraphicsEncoder>(*this, context.get()));
    }
}
