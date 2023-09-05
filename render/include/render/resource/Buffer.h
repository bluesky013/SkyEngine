//
// Created by Zach Lee on 2023/9/1.
//

#pragma once

#include <vector>
#include <rhi/Device.h>
#include <core/platform/Platform.h>

namespace sky {

    class Buffer {
    public:
        Buffer();
        virtual ~Buffer();

        void Init(uint32_t size, rhi::BufferUsageFlags usage, rhi::MemoryType memoryType);

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

        template <typename T>
        void Write(uint32_t offset, const T& val)
        {
            SKY_ASSERT(offset + sizeof(T) <= bufferDesc.size);
            new (ptr + offset) T(val);
            dirty = true;
        }

    protected:
        bool dirty = true;
        uint8_t *ptr = nullptr;
        std::vector<uint8_t> data;
    };
    using RDUniformBufferPtr = std::shared_ptr<UniformBuffer>;

    class DynamicUniformBuffer : public UniformBuffer {
    public:
        DynamicUniformBuffer() = default;
        ~DynamicUniformBuffer() override = default;

        bool Init(uint32_t size, uint32_t inflightCount);
        void Upload();

    private:
        uint32_t frameSize = 0;
        uint32_t totalSize = 0;
        uint32_t frameIndex = 0;
        uint32_t inflightCount = 1;
    };
    using RDDynamicUniformBuffer = std::shared_ptr<DynamicUniformBuffer>;

} // namespace sky
