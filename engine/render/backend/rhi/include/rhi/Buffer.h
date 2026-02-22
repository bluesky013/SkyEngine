//
// Created by Zach Lee on 2022/11/13.
//

#pragma once

#include <rhi/Core.h>
#include <memory>

namespace sky::rhi {

    class Buffer {
    public:
        Buffer()  = default;
        ~Buffer() = default;

        struct Descriptor {
            uint64_t                  size = 0;
            Flags<BufferUsageFlagBit> usage;
            MemoryType                memory = MemoryType::GPU_ONLY;
        };
        virtual uint8_t *Map() { return nullptr; }
        virtual void UnMap() {}

        const Descriptor &GetBufferDesc() const { return bufferDesc; }

    protected:
        Descriptor bufferDesc;
    };
    using BufferPtr = std::shared_ptr<Buffer>;

    struct BufferView {
        BufferPtr buffer;
        uint64_t offset = 0;
        uint64_t range  = 0;
    };
} // namespace sky::rhi
