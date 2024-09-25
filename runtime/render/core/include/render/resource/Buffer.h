//
// Created by Zach Lee on 2023/9/1.
//

#pragma once

#include <vector>
#include <rhi/Device.h>
#include <rhi/Queue.h>
#include <core/file/FileSystem.h>
#include <core/platform/Platform.h>
#include <render/RenderResource.h>

namespace sky {

    class Buffer : public IStreamableResource {
    public:
        Buffer();
        ~Buffer() override;

        void Init(uint64_t size, const rhi::BufferUsageFlags& usage, rhi::MemoryType memoryType);
        void SetSourceData(const rhi::BufferUploadRequest &data);

        virtual uint64_t GetSize() const { return bufferDesc.size; }

        const rhi::BufferPtr &GetRHIBuffer() const { return buffer; }
        virtual rhi::BufferView MakeView() const;

    protected:
        uint64_t UploadImpl() override;

        rhi::Device *device = nullptr;
        rhi::Buffer::Descriptor bufferDesc = {};
        rhi::BufferPtr buffer;

        rhi::BufferUploadRequest sourceData;
    };
    using RDBufferPtr = CounterPtr<Buffer>;

    struct VertexBuffer {
        RDBufferPtr buffer;
        uint64_t offset = 0;
        uint64_t range  = 0;
        uint32_t stride = 0;

        rhi::BufferView MakeView() const;
    };

    struct IndexBuffer {
        RDBufferPtr buffer;
        uint64_t offset          = 0;
        uint64_t range           = 0;
        rhi::IndexType indexType = rhi::IndexType::U32;

        rhi::BufferView MakeView() const;
    };

    class UniformBuffer : public Buffer {
    public:
        UniformBuffer() = default;
        ~UniformBuffer() override = default;

        virtual bool Init(uint32_t size);
        virtual void Write(uint32_t offset, const uint8_t *ptr, uint32_t size);
        template <typename T>
        void WriteT(uint32_t offset, const T& val)
        {
            SKY_ASSERT(offset + sizeof(T) <= bufferDesc.size);
            Write(offset, reinterpret_cast<const uint8_t *>(&val), static_cast<uint32_t>(sizeof(T)));
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

        bool Init(uint32_t size) override;
        void Upload();
        void Upload(rhi::BlitEncoder &encoder) override;

        uint64_t GetOffset() const { return frameIndex * alignedFrameSize; }
        uint64_t GetSize() const override { return frameSize; }
    private:
        uint32_t frameSize = 0;
        uint32_t alignedFrameSize;
        uint32_t frameIndex = 0;
    };
    using RDDynamicUniformBufferPtr = CounterPtr<DynamicUniformBuffer>;

    class DynamicBuffer : public Buffer {
    public:
        DynamicBuffer() = default;
        ~DynamicBuffer() override = default;

        bool Init(uint32_t size, const rhi::BufferUsageFlags& usage);
        void Update(uint8_t *ptr, uint32_t offset, uint32_t size);

        void SwapBuffer();
        uint8_t *GetMapped() const;
        uint64_t GetOffset() const { return frameIndex * alignedFrameSize; }
        uint64_t GetSize() const override { return frameSize; }

        rhi::BufferView MakeView() const override;
    private:
        uint8_t *mapped = nullptr;
        uint32_t frameSize = 0;
        uint32_t alignedFrameSize;
        uint32_t frameIndex = 0;
    };
    using RDDynamicBuffer = CounterPtr<DynamicBuffer>;
} // namespace sky
