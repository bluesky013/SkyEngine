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
#if SKY_ENABLE_RESOURCE_NAME
            const char               *name   = nullptr;
#endif
        };

        Buffer() = default;
        ~Buffer() override = default;

        // map/unmap for host-visible buffers (CPU_TO_GPU / CPU_ONLY memory);
        // GPU_ONLY buffers return nullptr
        virtual uint8_t *Map() { return nullptr; }
        virtual void UnMap() {}
    };
    using BufferPtr = CounterPtr<Buffer>;
} // namespace sky::aurora

