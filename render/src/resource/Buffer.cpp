//
// Created by Zach Lee on 2023/9/1.
//

#include <render/resource/Buffer.h>
#include <render/RHI.h>

namespace sky {

    Buffer::Buffer()
    {
        device = RHI::Get()->GetDevice();
    }

    bool UniformBuffer::Init(uint32_t size)
    {
        bufferDesc.size   = size;
        bufferDesc.usage  = rhi::BufferUsageFlagBit::UNIFORM;
        bufferDesc.memory = rhi::MemoryType::CPU_TO_GPU;

        data.resize(size, 0);
        ptr = data.data();

        buffer = device->CreateBuffer(bufferDesc);
        return static_cast<bool>(buffer);
    }

    void UniformBuffer::Update()
    {
        if (dirty) {
            dirty = false;
        }
    }

} // namespace sky