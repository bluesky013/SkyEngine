//
// Created by Zach Lee on 2022/5/27.
//


#include <render/resources/Buffer.h>
#include <render/DriverManager.h>
#include <framework/asset/AssetManager.h>
#include <fstream>

namespace sky {

    Buffer::Buffer(const Descriptor& desc)
    {
        Init(desc);
    }

    void Buffer::Init(const Descriptor& desc)
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

        drv::Buffer::Descriptor desc = {};
        desc.size = descriptor.size;
        desc.memory = descriptor.memory;
        desc.usage = descriptor.usage;
        rhiBuffer = DriverManager::Get()->GetDevice()->CreateDeviceObject<drv::Buffer>(desc);
    }

    bool Buffer::IsValid() const
    {
        return static_cast<bool>(rhiBuffer);
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

    const uint8_t* Buffer::Data() const
    {
        return rawData.data();
    }

    drv::BufferPtr Buffer::GetRHIBuffer() const
    {
        return rhiBuffer;
    }

    BufferView::BufferView(RDBufferPtr b, VkDeviceSize sz, VkDeviceSize o, uint32_t s)
        : buffer(std::move(b))
        , size(sz)
        , offset(o)
        , stride(s)
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

    void BufferView::WriteImpl(const uint8_t* data, uint64_t size, uint64_t offset)
    {
        if (buffer) {
            buffer->Write(data, size, offset);
        }
    }


    DynamicBufferView::DynamicBufferView(RDBufferPtr buf, VkDeviceSize sz, VkDeviceSize off, uint32_t frame, uint32_t block)
            : BufferView(std::move(buf), sz, off, static_cast<uint32_t>(sz))
            , frameNum(frame)
            , blockStride(block)
            , bufferIndex(~(0u))
            , currentFrame(0)
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
        currentFrame = (currentFrame + 1) % frameNum;
        dynamicOffset = static_cast<uint32_t>(offset) + currentFrame * blockStride;
    }

    void DynamicBufferView::RequestUpdate()
    {
        if (isDirty) {
            BufferView::RequestUpdate();
            SwapBuffer();
            isDirty = false;
        }
    }

    void DynamicBufferView::WriteImpl(const uint8_t* data, uint64_t size, uint64_t off)
    {
        if (buffer) {
            buffer->Write(data, size, off + dynamicOffset);
            isDirty = true;
        }
    }

    namespace impl {
        void LoadFromPath(const std::string& path, BufferAssetData& data)
        {
            auto realPath = AssetManager::Get()->GetRealPath(path);
            std::ifstream file(realPath,  std::ios::binary);
            if (!file.is_open()) {
                return;
            }
            cereal::BinaryInputArchive archive(file);
            archive >> data;
        }

        void SaveToPath(const std::string& path, const BufferAssetData& data)
        {
            std::ofstream file(path, std::ios::binary);
            if (!file.is_open()) {
                return;
            }
            cereal::BinaryOutputArchive binOutput(file);
            binOutput << data;
        }

        RDBufferPtr CreateFromData(const BufferAssetData& data)
        {
            Buffer::Descriptor bufferDesc = {};
            bufferDesc.size = data.data.size();
            bufferDesc.usage = data.usage;
            bufferDesc.memory = data.memory;
            auto buffer = std::make_shared<Buffer>(bufferDesc);
            buffer->InitRHI();
            buffer->Update(data.data.data(), data.data.size());
            return buffer;
        }
    }
}