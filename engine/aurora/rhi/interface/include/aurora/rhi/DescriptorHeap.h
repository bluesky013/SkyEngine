//
// Created on 2026/09/13.
//

#pragma once

#include <core/template/ReferenceObject.h>
#include <aurora/rhi/DescriptorEncoder.h>

#include <cstdint>

namespace sky::aurora {

    // Descriptor heap (VK_EXT_descriptor_heap): two backing buffers (resource
    // heap + sampler heap), per-type descriptor stride, index-based allocation.
    // Bindless tier2 only; CreateDescriptorHeap returns nullptr when the
    // extension is unsupported. The untyped path does not support combined
    // image samplers (separate sampled image + sampler only).
    class DescriptorHeap : public RefObject, public IDelayReleaseResource {
    public:
        struct Descriptor {
            uint32_t maxTextures = 0; // sampled / storage image
            uint32_t maxBuffers  = 0; // uniform / storage buffer
            uint32_t maxSamplers = 0;
        };

        // per-type indices (not a single unified index)
        struct Allocation {
            uint32_t texFirst = 0, texCount = 0;
            uint32_t bufFirst = 0, bufCount = 0;
            uint32_t smpFirst = 0, smpCount = 0;
        };

        DescriptorHeap()           = default;
        ~DescriptorHeap() override = default;

        virtual Allocation Allocate(const Descriptor &desc)   = 0;
        virtual void       Free(const Allocation &allocation) = 0;
        // Write descriptor blobs (vkWriteResourceDescriptorsEXT) into the
        // allocated index range.
        virtual void Update(const Allocation &allocation, DescriptorEncoder &encoder) = 0;
    };

    using DescriptorHeapPtr = CounterPtr<DescriptorHeap>;

} // namespace sky::aurora
