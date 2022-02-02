//
// Created by Zach Lee on 2022/2/1.
//

#include <engine/render/RenderBufferPool.h>
#include <engine/render/DriverManager.h>

namespace sky {

    RenderBufferPool::RenderBufferPool(Descriptor desc)
        : currentFrame(0)
        , descriptor(std::move(desc))
    {
        validBlockSize = descriptor.blockSize / descriptor.frame;
    }

    void RenderBufferPool::Reserve(uint32_t size)
    {
        auto num = std::ceil(size * descriptor.stride / (float)validBlockSize);
        if (num > blocks.size()) {
            AllocateBlock(num - blocks.size());
        }
    }

    RenderBufferPool::BufferHandle RenderBufferPool::GetBuffer(uint32_t index) const
    {
        uint32_t bufferIndex = index * descriptor.stride;
        uint32_t blockIndex = bufferIndex / validBlockSize;
        uint32_t offset = bufferIndex % validBlockSize + currentFrame * validBlockSize;
        return BufferHandle{blocks[blockIndex].buffer, offset};
    }

    void RenderBufferPool::SwapBuffer()
    {
        currentFrame = (currentFrame + 1) % descriptor.frame;
    }

    void RenderBufferPool::AllocateBlock(uint32_t num)
    {
        auto dev = DriverManager::Get()->GetDevice();
        for (uint32_t i = 0; i < num; ++i) {
            drv::Buffer::Descriptor bufferDesc = {};
            bufferDesc.size = descriptor.blockSize;
            bufferDesc.memory = descriptor.memory;
            bufferDesc.usage = descriptor.usage;
            auto buffer = dev->CreateDeviceObject<drv::Buffer>(bufferDesc);
            auto ptr = buffer->Map();
            blocks.emplace_back(Block{buffer, ptr});
        }
    }

    uint8_t* RenderBufferPool::GetAddress(uint32_t index)
    {
        uint32_t bufferIndex = index * descriptor.stride;
        uint32_t blockIndex = bufferIndex / validBlockSize;
        uint32_t offset = bufferIndex % validBlockSize + currentFrame * validBlockSize;
        return blocks[blockIndex].ptr + offset;
    }
}