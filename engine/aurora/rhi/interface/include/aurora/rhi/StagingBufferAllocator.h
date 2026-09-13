//
// Created on 2026/09/13.
//

#pragma once

#include <aurora/rhi/Buffer.h>

#include <vector>

namespace sky::aurora {

    class Device;

    // Per-frame staging allocator for same-frame uploads. Holds a ring of
    // CPU_TO_GPU segments (one per in-flight frame); Allocate bumps within the
    // current segment, Reset advances to the next segment. This guarantees a
    // segment is only rewritten after numFramesInFlight frames, so in-flight
    // copies from previous frames are never overwritten.
    class StagingBufferAllocator {
    public:
        struct Allocation {
            Buffer  *buffer = nullptr;
            uint64_t offset = 0;
            uint8_t *mapped = nullptr;
        };

        StagingBufferAllocator()  = default;
        ~StagingBufferAllocator() = default;

        bool       Init(Device *inDevice, uint64_t inSegmentSize, uint32_t inNumFrames = 1);
        Allocation Allocate(uint64_t inSize, uint64_t align);
        void       Reset(); // advance to the next in-flight segment

    private:
        struct Segment {
            BufferPtr buffer;
            uint8_t  *mapped = nullptr;
            uint64_t  offset = 0;
            uint64_t  size   = 0;
        };

        Device   *device = nullptr;
        std::vector<Segment> segments;
        uint32_t  current     = 0;
        uint32_t  numFrames   = 1;
        uint64_t  segmentSize = 0;
    };

} // namespace sky::aurora
