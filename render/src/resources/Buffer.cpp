//
// Created by Zach Lee on 2022/5/27.
//


#include <render/resources/Buffer.h>
#include <render/DriverManager.h>

namespace sky {

    Buffer::Buffer(const Descriptor& desc) : descriptor(desc)
    {
        if (desc.allocCPU) {
            rawData.resize(desc.size);
        }
    }

    void Buffer::InitRHI()
    {
        if (rhiBuffer) {
            return;
        }

        drv::Buffer::Descriptor desc = {};
        desc.size = descriptor.size;
        desc.memory = descriptor.memory;
        desc.usage = descriptor.usage;
        rhiBuffer = DriverManager::Get()->GetDevice()->CreateDeviceObject<drv::Buffer>(desc);
    }

    bool Buffer::IsValid() const
    {
        return !!rhiBuffer;
    }

    void Buffer::Write(const uint8_t* data, uint64_t size, uint64_t offset)
    {
        if (size + offset > rawData.size()) {
            return;
        }
        uint8_t* ptr = rawData.data() + offset;
        memcpy(ptr, data, size);
        dirty = true;
    }

    void Buffer::Update(const uint8_t* data, uint64_t srcSize)
    {
        if (!dirty) {
            return;
        }

        uint64_t validateSize = std::min(srcSize, descriptor.size);

        if (descriptor.memory == VMA_MEMORY_USAGE_GPU_ONLY) {
            auto device = DriverManager::Get()->GetDevice();
            auto queue = device->GetQueue(VK_QUEUE_GRAPHICS_BIT);

            drv::Buffer::Descriptor stagingDes = {};
            stagingDes.memory = VMA_MEMORY_USAGE_CPU_TO_GPU;
            stagingDes.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            stagingDes.size = validateSize;
            auto stagingBuffer = device->CreateDeviceObject<drv::Buffer>(stagingDes);
            uint8_t* dst = stagingBuffer->Map();
            memcpy(dst, data, validateSize);
            stagingBuffer->UnMap();

            auto cmd = queue->AllocateCommandBuffer({});
            cmd->Begin();

            cmd->Copy(stagingBuffer, rhiBuffer, VkBufferCopy{0, 0, descriptor.size});

            cmd->End();
            cmd->Submit(*queue, {});
            cmd->Wait();
        } else {
            uint8_t* dst = rhiBuffer->Map();
            memcpy(dst, data, validateSize);
            rhiBuffer->UnMap();
        }

        dirty = false;
    }

    void Buffer::Update(bool release)
    {
        Update(rawData.data(), rawData.size());
        if (release) {
            rawData.clear();
        }
    }

    drv::BufferPtr Buffer::GetRHIBuffer() const
    {
        return rhiBuffer;
    }

    BufferView::BufferView(RDBufferPtr b, uint32_t sz, uint32_t o, uint32_t s)
        : buffer(b)
        , size(sz)
        , offset(o)
        , stride(s)
    {
    }

    RDBufferPtr BufferView::GetBuffer() const
    {
        return buffer;
    }

    uint32_t BufferView::GetOffset() const
    {
        return offset;
    }

    uint32_t BufferView::GetSize() const
    {
        return size;
    }

    uint32_t BufferView::GetStride() const
    {
        return stride;
    }

    bool BufferView::IsValid() const
    {
        return buffer && buffer->IsValid();
    }

    void BufferView::RequestUpdate()
    {
        if (IsValid()) {
            buffer->Update();
        }
    }

}