//
// Aurora RenderViewport: a renderable surface a frame can be presented to.
//
// The base class describes only the surface contract (no swapchain, no frame
// index, no fence); concrete subclasses (e.g. ClientViewport) own the backing
// swapchain and per-image sync primitives. The frame index / fences live in the
// global DeviceFrameContext so multiple viewports share one in-flight frame.
//

#pragma once

#include <core/name/Name.h>
#include <aurora/rhi/Core.h>

namespace sky::aurora {

    class Image;
    class Semaphore;

    class RenderViewport {
    public:
        RenderViewport()          = default;
        virtual ~RenderViewport() = default;

        // Frame begin: self-check the surface/swapchain state and rebuild when
        // out-of-date. Returns false if the surface is unavailable (rebuild
        // failed / lost) — the frame's present is cancelled.
        virtual bool Begin() = 0;

        // Acquire the next backbuffer (called during RDG resource prepare).
        virtual bool Acquire() = 0;

        // Present the frame. No-op when the frame was cancelled.
        virtual void Release() = 0;

        // Valid between a successful Acquire() and Release().
        virtual Image       *GetBackbuffer() const = 0;
        virtual PixelFormat  GetFormat() const = 0;
        virtual Extent2D     GetExtent() const = 0;
        virtual const Name  &GetName() const = 0;

        // Per-current-image sync primitives wired into the frame driver's Submit.
        virtual Semaphore *GetAcquireSemaphore() const = 0;
        virtual Semaphore *GetRenderDoneSemaphore() const = 0;
    };

} // namespace sky::aurora
