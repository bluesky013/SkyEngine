//
// Created by blues on 2023/10/8.
//

#include <rhi/BufferPool.h>
#include <rhi/Device.h>
#include <core/util/Memory.h>

namespace sky::rhi {

    StagingBufferPool::~StagingBufferPool()
    {
        if (buffer && ptr != nullptr) {
            buffer->UnMap();
        }
    }

    bool StagingBufferPool::Init(sky::rhi::Device &dev, uint64_t size)
    {
        rhi::Buffer::Descriptor desc = {};
        desc.size = size;
        desc.usage = rhi::BufferUsageFlagBit::TRANSFER_SRC;
        desc.memory = MemoryType::CPU_TO_GPU;

        buffer = dev.CreateBuffer(desc);

        currentOffset = 0;
        capacity = size;

        ptr = buffer->Map();

        return static_cast<bool>(buffer);
    }

    void StagingBufferPool::Reset()
    {
        currentOffset = 0;
    }

    StagingBufferPool::View StagingBufferPool::Allocate(uint64_t size, uint64_t alignment)
    {
        uint64_t allocSize = Align(size, alignment);
        if (currentOffset + allocSize > capacity) {
            return {0, nullptr};
        }

        StagingBufferPool::View view{currentOffset, ptr + currentOffset};
        currentOffset += allocSize;
        return view;
    }

} // namespace sky::rhi