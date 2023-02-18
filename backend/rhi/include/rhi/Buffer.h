//
// Created by Zach Lee on 2022/11/13.
//

#pragma once

#include <rhi/Core.h>
#include <rhi/BufferView.h>
#include <memory>

namespace sky::rhi {

    class Buffer {
    public:
        Buffer()  = default;
        ~Buffer() = default;

        struct Descriptor {
            uint64_t                  size = 0;
            Flags<BufferUsageFlagBit> usage;
            MemoryType                memory      = MemoryType::GPU_ONLY;
            bool                      allocateMem = true;
        };
        virtual rhi::BufferViewPtr CreateView(const rhi::BufferViewDesc &desc) = 0;

    protected:
        Descriptor bufferDesc;
    };
    using BufferPtr = std::shared_ptr<Buffer>;
}
