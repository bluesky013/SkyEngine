//
// Created by Zach Lee on 2023/5/28.
//

#include <mtl/CommandBuffer.h>
#include <mtl/Device.h>

namespace sky::mtl {
    CommandBuffer::~CommandBuffer()
    {
        if (commandBuffer) {
            [commandBuffer release];
        }
    }

    bool CommandBuffer::Init(const Descriptor &desc)
    {
        auto *queue = static_cast<Queue*>(device.GetQueue(desc.queueType));
        commandBuffer = [queue->GetNativeHandle() commandBuffer];
        [commandBuffer retain];
        return true;
    }

    rhi::GraphicsEncoder &GraphicsEncoder::BeginPass(const rhi::PassBeginInfo &beginInfo)
    {
        return *this;
    }

    rhi::GraphicsEncoder &GraphicsEncoder::BindPipeline(const rhi::GraphicsPipelinePtr &pso)
    {
        return *this;
    }

    rhi::GraphicsEncoder &GraphicsEncoder::BindAssembly(const rhi::VertexAssemblyPtr &assembly)
    {
        return *this;
    }

    rhi::GraphicsEncoder &GraphicsEncoder::SetViewport(uint32_t count, const rhi::Viewport *viewport)
    {
        return *this;
    }

    rhi::GraphicsEncoder &GraphicsEncoder::SetScissor(uint32_t count, const rhi::Rect2D *scissor)
    {
        return *this;
    }

    rhi::GraphicsEncoder &GraphicsEncoder::DrawIndexed(const rhi::CmdDrawIndexed &indexed)
    {
        return *this;
    }

    rhi::GraphicsEncoder &GraphicsEncoder::DrawLinear(const rhi::CmdDrawLinear &linear)
    {
        return *this;
    }

    rhi::GraphicsEncoder &GraphicsEncoder::DrawIndirect(const rhi::BufferPtr &buffer, uint32_t offset, uint32_t size)
    {
        return *this;
    }

    rhi::GraphicsEncoder &GraphicsEncoder::BindSet(uint32_t id, const rhi::DescriptorSetPtr &set)
    {
        return *this;
    }

    rhi::GraphicsEncoder &GraphicsEncoder::NextSubPass()
    {
        return *this;
    }

    rhi::GraphicsEncoder &GraphicsEncoder::EndPass()
    {
        return *this;
    }
} // namespace sky::mtl
