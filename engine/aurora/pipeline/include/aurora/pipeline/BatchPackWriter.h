//
// Created on 2026/09/14.
//

#pragma once

#include <aurora/rdg/BatchAllocator.h>

#include <cstdint>
#include <type_traits>

namespace sky::aurora {

    // BatchPackWriter: pipeline-layer helper that packs per-object uniform
    // structs into a BatchAllocator and returns the dynamic offset
    // (== pack offset, feed directly into DrawItem::batchDynamicOffset).
    // Structure layout knowledge lives here; the rhi-layer BatchAllocator
    // stays a plain byte allocator.
    class BatchPackWriter {
    public:
        explicit BatchPackWriter(BatchAllocator &alloc)
            : mAlloc(alloc)
        {
        }

        template <typename T>
        uint32_t Pack(const T &data)
        {
            static_assert(std::is_trivially_copyable_v<T>, "packed batch struct must be trivially copyable");
            const uint32_t size   = static_cast<uint32_t>(sizeof(T));
            const uint32_t offset = mAlloc.Allocate(size);
            if (offset == UINT32_MAX) {
                return offset;
            }
            mAlloc.Write(offset, &data, size);
            return offset;
        }

    private:
        BatchAllocator &mAlloc;
    };

} // namespace sky::aurora
