//
// Created by Zach Lee on 2022/5/27.
//

#include <framework/asset/AssetManager.h>
#include <fstream>
#include <render/DevObjManager.h>
#include <render/RHIManager.h>
#include <render/resources/Buffer.h>

namespace sky {

    Buffer::Buffer(const Descriptor &desc)
    {
        Init(desc);
    }

    Buffer::~Buffer()
    {
        if (rhiBuffer) {
            if (mapPtr != nullptr) {
                rhiBuffer->UnMap();
            }
            DevObjManager::Get()->FreeDeviceObject(rhiBuffer);
        }
    }

    void Buffer::Init(const Descriptor &desc)
    {
        descriptor = desc;
        if (desc.allocCPU) {
            rawData.resize(desc.size);
        }
    }

    void Buffer::InitRHI()
    {
        if (rhiBuffer) {
            return;
        }

        vk::Buffer::VkDescriptor desc = {};
        desc.size                    = realSize = descriptor.size * descriptor.inflight;
        desc.memory                  = descriptor.memory;
        desc.usage                   = descriptor.usage;
        rhiBuffer                    = RHIManager::Get()->GetDevice()->CreateDeviceObject<vk::Buffer>(desc);
        if (descriptor.keepMap) {
            mapPtr = rhiBuffer->Map();
        }
    }

    bool Buffer::IsValid() const
    {
        return static_cast<bool>(rhiBuffer);
    }

    uint8_t *Buffer::GetMappedAddress() const
    {
        return mapPtr;
    }

    void Buffer::Write(const uint8_t *data, uint64_t size, uint64_t offset)
    {
        if (size + offset > rawData.size()) {
            return;
        }
        uint8_t *ptr = rawData.data() + offset;
        memcpy(ptr, data, size);
    }

    void Buffer::Update(const uint8_t *data, uint64_t range, uint64_t offset)
    {
        if (range + offset > realSize) {
            return;
        }

        if (descriptor.memory == VMA_MEMORY_USAGE_GPU_ONLY) {
            auto device = RHIManager::Get()->GetDevice();
            auto queue  = device->GetGraphicsQueue();

            vk::Buffer::VkDescriptor stagingDes = {};
            stagingDes.memory                  = VMA_MEMORY_USAGE_CPU_TO_GPU;
            stagingDes.usage                   = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            stagingDes.size                    = range;
            auto     stagingBuffer             = device->CreateDeviceObject<vk::Buffer>(stagingDes);
            uint8_t *dst                       = stagingBuffer->Map();
            memcpy(dst, data, range);
            stagingBuffer->UnMap();

            auto cmd = queue->AllocateCommandBuffer({});
            cmd->Begin();

            cmd->Copy(stagingBuffer, rhiBuffer, VkBufferCopy{0, offset, range});

            cmd->End();
            cmd->Submit(*queue, {});
            cmd->Wait();
        } else {
            uint8_t *dst = rhiBuffer->Map() + offset;
            memcpy(dst, data, range);
            rhiBuffer->UnMap();
        }
    }

    void Buffer::Update(bool release)
    {
        Update(0, 0, descriptor.size, release);
        if (release) {
            rawData.clear();
        }
    }

    void Buffer::Update(uint64_t offset, uint64_t rawOffset, uint64_t range, bool release)
    {
        Update(rawData.data() + rawOffset, range, offset);
        if (release) {
            rawData.clear();
        }
    }

    const uint8_t *Buffer::Data() const
    {
        return rawData.data();
    }

    vk::BufferPtr Buffer::GetRHIBuffer() const
    {
        return rhiBuffer;
    }

    BufferView::BufferView(RDBufferPtr b, VkDeviceSize sz, VkDeviceSize o, uint32_t s) : buffer(std::move(b)), size(sz), offset(o), stride(s)
    {
    }

    RDBufferPtr BufferView::GetBuffer() const
    {
        return buffer;
    }

    VkDeviceSize BufferView::GetOffset() const
    {
        return offset;
    }

    VkDeviceSize BufferView::GetSize() const
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

    void BufferView::WriteImpl(const uint8_t *data, uint64_t size, uint64_t off)
    {
        if (buffer) {
            buffer->Write(data, size, offset + off);
        }
    }

    DynamicBufferView::DynamicBufferView(RDBufferPtr buf, VkDeviceSize sz, VkDeviceSize off, uint32_t frame, uint32_t block)
        : BufferView(std::move(buf), sz, off, static_cast<uint32_t>(sz))
        , frameNum(frame)
        , blockStride(block)
        , bufferIndex(~(0u))
        , currentFrame(frameNum - 1)
        , dynamicOffset(0)
        , isDirty(false)
    {
    }

    uint32_t DynamicBufferView::GetDynamicOffset() const
    {
        return dynamicOffset;
    }

    void DynamicBufferView::SetID(uint32_t id)
    {
        bufferIndex = id;
    }

    uint32_t DynamicBufferView::GetID() const
    {
        return bufferIndex;
    }

    void DynamicBufferView::SwapBuffer()
    {
        currentFrame  = (currentFrame + 1) % frameNum;
        dynamicOffset = currentFrame * blockStride;
    }

    void DynamicBufferView::RequestUpdate()
    {
        if (isDirty) {
            SwapBuffer();
            if (IsValid()) {
                buffer->Update(offset + dynamicOffset, offset, size);
            }
            isDirty = false;
        }
    }

    void DynamicBufferView::WriteImpl(const uint8_t *data, uint64_t size, uint64_t off)
    {
        if (IsValid()) {
            buffer->Write(data, size, off + offset);
            isDirty = true;
        }
    }

    RDBufferPtr Buffer::CreateFromData(const BufferAssetData &data)
    {
        Buffer::Descriptor bufferDesc = {};
        bufferDesc.size               = data.data.size();
        bufferDesc.usage              = data.usage;
        bufferDesc.memory             = data.memory;
        auto buffer  = std::make_shared<Buffer>(bufferDesc);
        buffer->InitRHI();
        buffer->Update(data.data.data(), data.data.size());
        return buffer;
    }
} // namespace sky
