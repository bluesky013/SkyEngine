//
// BatchAllocator: per-frame dynamic uniform buffer for batch (set 2) data.
// Single big host-visible buffer; offsets are 256B aligned; reset each frame.
// (Ring/multi-buffering for in-flight frames is a follow-up.)
//

#pragma once

#include <aurora/rhi/Buffer.h>

namespace sky::aurora {

    class Device;

    class BatchAllocator {
    public:
        static constexpr uint32_t OFFSET_ALIGNMENT = 256;

        BatchAllocator() = default;
        ~BatchAllocator() = default;

        BatchAllocator(const BatchAllocator &) = delete;
        BatchAllocator &operator=(const BatchAllocator &) = delete;

        bool Init(Device *device, uint32_t capacity = 1024 * 1024);

        // allocate size bytes; returns offset, or UINT32_MAX on exhaustion
        uint32_t Allocate(uint32_t size);

        // write data into a previously allocated range
        void Write(uint32_t offset, const void *data, uint32_t size);

        // frame-end reset
        void Reset();

        Buffer   *GetBuffer() const { return mBuffer.Get(); }
        uint32_t  GetUsedBytes() const { return mCursor; }
        uint32_t  GetCapacity() const { return mCapacity; }

    private:
        Device  *mDevice   = nullptr;
        BufferPtr mBuffer;
        uint8_t *mMapped   = nullptr;
        uint32_t mCursor   = 0;
        uint32_t mCapacity = 0;
    };

} // namespace sky::aurora
