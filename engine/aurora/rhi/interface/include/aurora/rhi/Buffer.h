//
// Created by blues on 2026/3/29.
//

#pragma once

#include <core/template/ReferenceObject.h>
#include <aurora/rhi/Core.h>
#include <aurora/rhi/Resource.h>

namespace sky::aurora {

    class Buffer
        : public RefObject
        , public IDelayReleaseResource {
    public:
        struct Descriptor {
            uint64_t                  size = 0;
            Flags<BufferUsageFlagBit> usage;
            MemoryType                memory = MemoryType::GPU_ONLY;
        };

        Buffer() = default;
        ~Buffer() override = default;
    };
    using BufferPtr = CounterPtr<Buffer>;
} // namespace sky::aurora

