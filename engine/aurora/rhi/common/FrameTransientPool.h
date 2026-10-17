//
// Created on 2026/10/16.
//

#pragma once

#include <aurora/rdg/FrozenFrameGraph.h>
#include <mutex>

namespace sky::aurora {
    class Device;

    struct FrameResourceBindings {
        // One lease pins all backing in this plan through GPU retirement; bindings are destroyed first.
        std::shared_ptr<const void> transientLease;
        std::vector<BufferPtr>      buffers;
        std::vector<FrameImageReference> images;
        // Canonical logical indices identify physical objects; native state is tracked once per backing.
        std::vector<uint32_t>              bufferBacking, imageBacking;
        std::vector<CounterPtr<RefObject>> objects;
        std::shared_ptr<const void>        payload;
    };
    template <class Encoder>
    struct FrameGpuPayload {
        FrameResourceBindings    resources;
        std::unique_ptr<Encoder> encoder;
    };
    struct FrameTransientPoolLimits {
        uint32_t objects = 512;
        uint64_t bytes   = 256ull * 1024 * 1024;
    };
    struct FrameTransientPoolStats {
        uint64_t allocations = 0, reuses = 0, evictions = 0, aliases = 0;
        uint64_t bytes   = 0;
        uint32_t objects = 0, borrowed = 0;
    };
    class FrameTransientPool final {
    public:
        explicit FrameTransientPool(Device &device, FrameTransientPoolLimits limits = {}) : device(device), limits(limits)
        {
        }
        // Called only with the matching successful semantic analysis; this allocates backing, not native access plans.
        std::variant<FrameError, FrameResourceBindings> Acquire(const FrameGraphData &, const FrameGraphAnalysis &, uint64_t deviceEpoch);
        FrameTransientPoolStats                         GetStats() const;

    private:
        struct Entry {
            Buffer::Descriptor        bufferDescriptor;
            Image::Descriptor         imageDescriptor;
            BufferPtr                 buffer;
            ImagePtr                  image;
            uint64_t                  bytes = 0, epoch = 0;
            std::weak_ptr<const void> borrower;
        };
        bool                     MakeRoom(uint64_t bytes);
        Device                  &device;
        FrameTransientPoolLimits limits;
        mutable std::mutex       mutex;
        std::vector<Entry>       entries;
        FrameTransientPoolStats  stats;
    };
} // namespace sky::aurora
