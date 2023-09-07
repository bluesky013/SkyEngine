//
// Created by Zach Lee on 2023/9/1.
//

#include <render/resource/Buffer.h>
#include <render/RHI.h>
#include <render/Renderer.h>
#include <core/util/Memory.h>

namespace sky {

    Buffer::~Buffer()
    {
        Renderer::Get()->GetResourceGC()->CollectBuffer(buffer);
    }

    Buffer::Buffer()
    {
        device = RHI::Get()->GetDevice();
    }

    void Buffer::Init(uint64_t size, rhi::BufferUsageFlags usage, rhi::MemoryType memoryType)
    {
        bufferDesc.size   = size;
        bufferDesc.usage  = rhi::BufferUsageFlagBit::UNIFORM;
        bufferDesc.memory = rhi::MemoryType::GPU_ONLY;

        buffer = device->CreateBuffer(bufferDesc);
    }

    bool UniformBuffer::Init(uint32_t size)
    {
        Buffer::Init(size, rhi::BufferUsageFlagBit::TRANSFER_DST | rhi::BufferUsageFlagBit::UNIFORM, rhi::MemoryType::GPU_ONLY);
        data.resize(size, 0);
        ptr = data.data();
        return static_cast<bool>(buffer);
    }

    void DynamicUniformBuffer::Upload()
    {
        if (!dirty) {
            return;
        }

        frameIndex = (frameIndex + 1) % inflightCount;

        auto *mapPtr = buffer->Map() + frameIndex * frameSize;
        memcpy(mapPtr, ptr, frameSize);
        buffer->UnMap();

        dirty = false;
    }

    bool DynamicUniformBuffer::Init(uint32_t size, uint32_t inflightCount)
    {
        frameSize = size;
        auto alignedFrameSize = Align(size, device->GetLimitations().minUniformBufferOffsetAlignment);

        Buffer::Init(alignedFrameSize * inflightCount, rhi::BufferUsageFlagBit::UNIFORM, rhi::MemoryType::CPU_TO_GPU);

        data.resize(size, 0);
        ptr = data.data();
        return static_cast<bool>(buffer);
    }

} // namespace sky
