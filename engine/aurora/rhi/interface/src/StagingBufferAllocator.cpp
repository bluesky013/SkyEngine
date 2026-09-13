//
// Created on 2026/09/13.
//

#include <aurora/rhi/StagingBufferAllocator.h>
#include <aurora/rhi/Device.h>

namespace sky::aurora {

    bool StagingBufferAllocator::Init(Device *inDevice, uint64_t inSegmentSize, uint32_t inNumFrames)
    {
        device      = inDevice;
        segmentSize = inSegmentSize;
        numFrames   = inNumFrames < 1 ? 1 : inNumFrames;

        segments.resize(numFrames);
        for (auto &segment : segments) {
            Buffer::Descriptor desc;
            desc.size   = segmentSize;
            desc.usage  = BufferUsageFlagBit::TRANSFER_SRC;
            desc.memory = MemoryType::CPU_TO_GPU;
            segment.buffer = BufferPtr(device->CreateBuffer(desc));
            if (segment.buffer == nullptr) {
                return false;
            }
            segment.mapped = segment.buffer->Map();
            if (segment.mapped == nullptr) {
                return false;
            }
            segment.offset = 0;
            segment.size   = segmentSize;
        }
        current = 0;
        return true;
    }

    StagingBufferAllocator::Allocation StagingBufferAllocator::Allocate(uint64_t inSize, uint64_t align)
    {
        Allocation alloc;
        if (segments.empty()) {
            return alloc;
        }

        auto &segment = segments[current];
        if (segment.mapped == nullptr) {
            return alloc;
        }

        const uint64_t alignedOffset = (segment.offset + align - 1) & ~(align - 1);
        if (alignedOffset + inSize > segment.size) {
            return alloc; // out of space for this frame's segment
        }
        segment.offset = alignedOffset + inSize;

        alloc.buffer = segment.buffer.Get();
        alloc.offset = alignedOffset;
        alloc.mapped = segment.mapped + alignedOffset;
        return alloc;
    }

    void StagingBufferAllocator::Reset()
    {
        if (segments.empty()) {
            return;
        }
        current = (current + 1) % numFrames;
        segments[current].offset = 0;
    }

} // namespace sky::aurora
