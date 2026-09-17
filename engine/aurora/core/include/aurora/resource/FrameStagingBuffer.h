//
// FrameStagingBuffer: same-frame staging upload. Wraps StagingBufferAllocator
// (a per-in-flight-frame ring of CPU_TO_GPU segments) and queues inline buffer
// copies that are flushed inside the current frame's command buffer via
// BlitEncoder::CopyBuffer. numFrames must match DeviceFrameContext::inflightNum;
// Reset() advances to the next in-flight segment once per frame.
//

#pragma once

#include <aurora/rhi/Encoder.h>
#include <aurora/rhi/StagingBufferAllocator.h>

#include <cstring>
#include <vector>

namespace sky::aurora {

    class Device;

    class FrameStagingBuffer {
    public:
        FrameStagingBuffer()  = default;
        ~FrameStagingBuffer() = default;

        bool Init(Device *dev, uint64_t segmentSize, uint32_t numFrames)
        {
            return allocator.Init(dev, segmentSize, numFrames);
        }

        // Host side: stage `data` into a staging slot and queue an inline copy
        // into `dst` at `dstOffset`. The copy is emitted by Flush().
        bool Upload(Buffer *dst, const void *data, uint64_t size, uint64_t dstOffset = 0)
        {
            auto alloc = allocator.Allocate(size, 1);
            if (alloc.mapped == nullptr) {
                return false;
            }
            std::memcpy(alloc.mapped, data, size);
            pending.push_back({alloc.buffer, alloc.offset, dst, dstOffset, size});
            return true;
        }

        // Render side (inside a COPYBLIT pass): emit all queued copies inline.
        void Flush(BlitEncoder &encoder)
        {
            for (const auto &p : pending) {
                encoder.CopyBuffer(p.src, p.dst, p.size, p.srcOffset, p.dstOffset);
            }
            pending.clear();
        }

        // Frame context hook: advance to the next in-flight segment (once per frame).
        void Reset()
        {
            allocator.Reset();
        }

    private:
        struct PendingCopy {
            Buffer  *src       = nullptr;
            uint64_t srcOffset = 0;
            Buffer  *dst       = nullptr;
            uint64_t dstOffset = 0;
            uint64_t size      = 0;
        };

        StagingBufferAllocator allocator;
        std::vector<PendingCopy> pending;
    };

} // namespace sky::aurora
