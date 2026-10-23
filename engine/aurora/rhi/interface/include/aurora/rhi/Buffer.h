//
// Created by blues on 2026/3/29.
//

#pragma once

#include <aurora/rhi/Core.h>
#include <aurora/rhi/Resource.h>
#include <core/template/ReferenceObject.h>

namespace sky::aurora {

    class Device;

    class Buffer : public RefObject, public IDelayReleaseResource {
    public:
        // Unknown owners cannot enter backend-specific native paths.
        virtual const Device *GetDevice() const noexcept
        {
            return nullptr;
        }

        struct Descriptor {
            uint64_t                  size = 0;
            Flags<BufferUsageFlagBit> usage;
            MemoryType                memory = MemoryType::GPU_ONLY;
#if SKY_ENABLE_RESOURCE_NAME
            const char *name = nullptr;
#endif
        };

        Buffer()           = default;
        ~Buffer() override = default;

        // map/unmap for host-visible buffers (CPU_TO_GPU / CPU_ONLY memory);
        // GPU_ONLY buffers return nullptr
        virtual uint8_t *Map()
        {
            return nullptr;
        }
        virtual void UnMap()
        {
        }

        // Allocated size in bytes; backends override to expose the real size.
        virtual uint64_t GetSize() const
        {
            return 0;
        }
        virtual const Descriptor &GetDescriptor() const
        {
            static const Descriptor unknown;
            return unknown;
        }
    };
    using BufferPtr = CounterPtr<Buffer>;
} // namespace sky::aurora
