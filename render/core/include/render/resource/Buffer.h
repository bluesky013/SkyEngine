//
// Created by Zach Lee on 2023/9/1.
//

#pragma once

#include <vector>
#include <rhi/Device.h>
#include <rhi/Queue.h>
#include <core/platform/Platform.h>

namespace sky {

    class Buffer {
    public:
        Buffer();
        virtual ~Buffer();

        void Init(uint64_t size, rhi::BufferUsageFlags usage, rhi::MemoryType memoryType);
        virtual void Resize(uint64_t size);
        virtual uint64_t GetRange() const { return bufferDesc.size; }

        const rhi::BufferPtr &GetRHIBuffer() const { return buffer; }

        rhi::TransferTaskHandle Upload(const std::string &path, rhi::Queue &queue, uint32_t offset);

    protected:
        rhi::Device *device = nullptr;
        rhi::Buffer::Descriptor bufferDesc = {};
        rhi::BufferPtr buffer;
    };
    using RDBufferPtr = std::shared_ptr<Buffer>;

    class UniformBuffer : public Buffer {
    public:
        UniformBuffer() = default;
        ~UniformBuffer() override = default;

        bool Init(uint32_t size);
        void Write(uint32_t offset, const uint8_t *ptr, uint32_t size);
        template <typename T>
        void Write(uint32_t offset, const T& val)
        {
            SKY_ASSERT(offset + sizeof(T) <= bufferDesc.size);
            new (ptr + offset) T(val);
            dirty = true;
        }

        virtual void Upload(rhi::BlitEncoder &encoder);

    protected:
        bool dirty = true;
        uint8_t *ptr = nullptr;
        std::vector<uint8_t> data;
        rhi::BufferPtr stagingBuffer;
    };
    using RDUniformBufferPtr = std::shared_ptr<UniformBuffer>;

    class DynamicUniformBuffer : public UniformBuffer {
    public:
        DynamicUniformBuffer() = default;
        ~DynamicUniformBuffer() override = default;

        bool Init(uint32_t size, uint32_t inflightCount);
        void Upload();
        void Upload(rhi::BlitEncoder &encoder) override;

        uint32_t GetOffset() const { return frameIndex * alignedFrameSize; }
        uint64_t GetRange() const override { return frameSize; }
    private:
        uint32_t frameSize = 0;
        uint32_t alignedFrameSize;
        uint32_t frameIndex = 0;
        uint32_t inflightCount = 1;
    };
    using RDDynamicUniformBufferPtr = std::shared_ptr<DynamicUniformBuffer>;

} // namespace sky
