//
// Shared ownership helpers for id<MTLSharedEvent> handles.
//
// Fence and semaphore both keep a retained MTLSharedEvent behind a void* (the
// public headers must not expose Obj-C types), so their create/destroy logic
// lives here. The Obj-C implementation is in MetalSync.mm.
//

#pragma once

#include <cstdint>

namespace sky::aurora {

    class MetalDevice;

    // Creates a retained id<MTLSharedEvent> stored into `outEvent` (void*).
    bool CreateMetalSharedEvent(MetalDevice &device, uint64_t initialValue, void *&outEvent);

    // Releases a handle produced by CreateMetalSharedEvent and nulls it.
    void ReleaseMetalSharedEvent(void *&event);

} // namespace sky::aurora
