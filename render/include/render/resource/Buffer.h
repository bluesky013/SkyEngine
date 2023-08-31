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
        virtual ~Buffer() = default;

    protected:
        rhi::Device *device = nullptr;
        rhi::Buffer::Descriptor bufferDesc = {};
        rhi::BufferPtr buffer;
    };

    class UniformBuffer : public Buffer {
    public:
        UniformBuffer() = default;
        ~UniformBuffer() override = default;

        bool Init(uint32_t size);
        void Update();

        template <typename T>
        void Write(uint32_t offset, const T& val)
        {
            SKY_ASSERT(offset + sizeof(T) <= bufferDesc.size);
            new (ptr + offset) T(val);
            dirty = true;
        }

    private:
        bool dirty = true;
        uint8_t *ptr = nullptr;
        std::vector<uint8_t> data;
    };

} // namespace sky