//
// Created by Zach Lee on 2023/9/1.
//

#include <render/resource/Buffer.h>
#include <core/util/Memory.h>
#include <rhi/Stream.h>
#include <render/RHI.h>
#include <render/Renderer.h>

namespace sky {

    Buffer::~Buffer()
    {
        Renderer::Get()->GetResourceGC()->CollectBuffer(buffer);
    }

    Buffer::Buffer()
    {
        device = RHI::Get()->GetDevice();
    }

    void Buffer::Init(uint64_t size, const rhi::BufferUsageFlags& usage, rhi::MemoryType memoryType)
    {
        bufferDesc.size   = size;
        bufferDesc.usage  = usage;
        bufferDesc.memory = memoryType;

        buffer = device->CreateBuffer(bufferDesc);
    }

    void Buffer::SetSourceData(const rhi::BufferUploadRequest &data)
    {
        sourceData = data;
    }

    void Buffer::Resize(uint64_t size)
    {
        Renderer::Get()->GetResourceGC()->CollectBuffer(buffer);
        bufferDesc.size = size;
        buffer = device->CreateBuffer(bufferDesc);
    }

    uint64_t Buffer::UploadImpl()
    {
        uploadHandle = uploadQueue->UploadBuffer(buffer, sourceData);
        sourceData.source = nullptr;
        return sourceData.size;
    }

    rhi::BufferView Buffer::MakeView() const
    {
        return rhi::BufferView{buffer, 0, bufferDesc.size};
    }

    rhi::BufferView VertexBuffer::MakeView() const
    {
        return rhi::BufferView{buffer->GetRHIBuffer(), offset, range};
    }

    rhi::BufferView IndexBuffer::MakeView() const
    {
        return rhi::BufferView{buffer->GetRHIBuffer(), offset, range};
    }

    bool UniformBuffer::Init(uint32_t size)
    {
        Buffer::Init(size, rhi::BufferUsageFlagBit::UNIFORM | rhi::BufferUsageFlagBit::TRANSFER_DST, rhi::MemoryType::GPU_ONLY);

        rhi::Buffer::Descriptor desc = {};
        desc.size = size;
        desc.memory = rhi::MemoryType::CPU_TO_GPU;
        desc.usage = rhi::BufferUsageFlagBit::UNIFORM | rhi::BufferUsageFlagBit::TRANSFER_SRC;
        stagingBuffer = RHI::Get()->GetDevice()->CreateBuffer(desc);

        data.resize(size);
        ptr = data.data();
        return static_cast<bool>(buffer);
    }

    void UniformBuffer::Write(uint32_t offset, const uint8_t *src, uint32_t size)
    {
        SKY_ASSERT(offset + size <= bufferDesc.size);
        memcpy(ptr + offset, src, size);
        dirty = true;
    }

    void UniformBuffer::Upload(rhi::BlitEncoder &encoder)
    {
        uint8_t *tmp = stagingBuffer->Map();
        memcpy(tmp, data.data(), data.size());
        stagingBuffer->UnMap();

        encoder.CopyBuffer(stagingBuffer, buffer, data.size(), 0, 0);
    }

    void DynamicUniformBuffer::Upload()
    {
        if (!dirty) {
            return;
        }

        frameIndex = (frameIndex + 1) % Renderer::Get()->GetInflightFrameCount();

        auto *mapPtr = buffer->Map() + frameIndex * alignedFrameSize;
        memcpy(mapPtr, ptr, alignedFrameSize);
        buffer->UnMap();

        dirty = false;
    }

    void DynamicUniformBuffer::Upload(rhi::BlitEncoder &/*encoder*/)
    {
        Upload();
    }

    bool DynamicUniformBuffer::Init(uint32_t size)
    {
        frameSize = size;
        alignedFrameSize = Align(size, device->GetLimitations().minUniformBufferOffsetAlignment);

        Buffer::Init(alignedFrameSize * Renderer::Get()->GetInflightFrameCount(), rhi::BufferUsageFlagBit::UNIFORM, rhi::MemoryType::CPU_TO_GPU);

        data.resize(frameSize, 0);
        ptr = data.data();
        return static_cast<bool>(buffer);
    }

} // namespace sky
