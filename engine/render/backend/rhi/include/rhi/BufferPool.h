//
// Created by blues on 2023/10/8.
//

#pragma once

#include <rhi/Buffer.h>
#include <core/template/Result.h>

namespace sky::rhi {
    class Device;

    class StagingBufferPool {
    public:
        StagingBufferPool() = default;
        ~StagingBufferPool();

        struct View {
            uint64_t offset = 0;
            uint8_t *ptr = nullptr;
        };

        bool Init(Device &dev, uint64_t size);
        void Reset();

        View Allocate(uint64_t size, uint64_t alignment);
        const BufferPtr &GetBuffer() const { return buffer; }

    private:
        BufferPtr buffer;
        uint8_t *ptr = nullptr;
        uint64_t currentOffset = 0;
        uint64_t capacity = 0;
    };

} // namespace sky::rhi
