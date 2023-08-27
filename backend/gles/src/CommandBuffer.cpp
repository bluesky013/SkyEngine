//
// Created by Zach on 2023/1/31.
//

#include <gles/CommandBuffer.h>
#include <gles/Queue.h>
#include <gles/FrameBuffer.h>
#include <gles/Device.h>
#include <gles/Core.h>

namespace sky::gles {

    rhi::GraphicsEncoder &GraphicsEncoder::BeginQuery(const rhi::QueryPoolPtr &query, uint32_t id)
    {
        return *this;
    }

    rhi::GraphicsEncoder &GraphicsEncoder::EndQuery(const rhi::QueryPoolPtr &query, uint32_t id)
    {
        return *this;
    }

    rhi::GraphicsEncoder &GraphicsEncoder::WriteTimeStamp(const rhi::QueryPoolPtr &query, rhi::PipelineStageBit stage, uint32_t id)
    {
        return *this;
    }

    rhi::GraphicsEncoder &GraphicsEncoder::BeginPass(const rhi::PassBeginInfo &beginInfo)
    {
        uint32_t copySize = beginInfo.clearCount * sizeof(rhi::ClearValue);
        auto *copyValues = reinterpret_cast<rhi::ClearValue*>(commandBuffer.Allocate(copySize));
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
        auto *copyValues = reinterpret_cast<rhi::Viewport*>(commandBuffer.Allocate(copySize));
        memcpy(copyValues, viewport, copySize);
        commandBuffer.EnqueueMessage(&CommandContext::CmdSetViewport, context, count, copyValues);
        return *this;
    }

    rhi::GraphicsEncoder &GraphicsEncoder::SetScissor(uint32_t count, const rhi::Rect2D *scissor)
    {
        uint32_t copySize = count * sizeof(rhi::Rect2D);
        auto *copyValues = reinterpret_cast<rhi::Rect2D*>(commandBuffer.Allocate(copySize));
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

    rhi::GraphicsEncoder &GraphicsEncoder::DrawIndexedIndirect(const rhi::BufferPtr &buffer, uint32_t offset, uint32_t count, uint32_t stride)
    {
        commandBuffer.EnqueueMessage(&CommandContext::CmdDrawIndexedIndirect, context,
                                     std::static_pointer_cast<Buffer>(buffer),
                                     offset, count, stride);
        return *this;
    }

    rhi::GraphicsEncoder &GraphicsEncoder::DrawIndirect(const rhi::BufferPtr &buffer, uint32_t offset, uint32_t count, uint32_t stride)
    {
        commandBuffer.EnqueueMessage(&CommandContext::CmdDrawIndirect, context,
                                     std::static_pointer_cast<Buffer>(buffer),
                                     offset, count, stride);
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

    rhi::GraphicsEncoder &GraphicsEncoder::NextSubPass()
    {
        commandBuffer.EnqueueMessage(&CommandContext::CmdNextPass, context);
        return *this;
    }

    rhi::GraphicsEncoder &GraphicsEncoder::EndPass()
    {
        commandBuffer.EnqueueMessage(&CommandContext::CmdEndPass, context);
        return *this;
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

    rhi::BlitEncoder &BlitEncoder::BlitTexture(const rhi::ImagePtr &src, const rhi::ImagePtr &dst, const std::vector<rhi::BlitInfo> &blitInputs, rhi::Filter filter)
    {
//        glBlitFramebuffer()
        return *this;
    }

    rhi::BlitEncoder &BlitEncoder::ResoleTexture(const rhi::ImagePtr &src, const rhi::ImagePtr &dst, const std::vector<rhi::ResolveInfo> &resolves)
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
        auto storage = std::make_unique<LinearStorage>(DEFAULT_SIZE);
        iterator = storages.emplace(iterator, std::move(storage));
    }

    bool CommandBuffer::Init(const Descriptor &desc)
    {
        AllocateStorage();
        context = std::make_unique<CommandContext>();
        return true;
    }

    void CommandBuffer::Execute()
    {
        while (head != nullptr) {
            auto *ptr = head->next;
            head->Execute();
            head->~TaskBase();
            head = ptr;
        }
    }

    void CommandBuffer::Begin()
    {
        Reset();
    }

    void CommandBuffer::End()
    {
    }

    void CommandBuffer::Submit(rhi::Queue &queue, const rhi::SubmitInfo &info)
    {
        auto &glesQueue = static_cast<Queue&>(queue);
        context->Attach(glesQueue);
        glesQueue.CreateTask([this, info]() {
            Execute();
            std::static_pointer_cast<Fence>(info.fence)->Signal();
        });
    }

    std::shared_ptr<rhi::GraphicsEncoder> CommandBuffer::EncodeGraphics()
    {
        return std::static_pointer_cast<rhi::GraphicsEncoder>(std::make_shared<GraphicsEncoder>(*this, context.get()));
    }

    std::shared_ptr<rhi::BlitEncoder> CommandBuffer::EncodeBlit()
    {
        return std::static_pointer_cast<rhi::BlitEncoder>(std::make_shared<BlitEncoder>(*this, context.get()));
    }
}
