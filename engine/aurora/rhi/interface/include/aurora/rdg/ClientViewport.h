//
// Aurora ClientViewport: a swapchain-backed window viewport.
//
// Owns the SwapChain and a ring of per-frame binary semaphores (acquire /
// render-done). The frame index and fences live in the global DeviceFrameContext
// so multiple viewports share one in-flight frame; the semaphore ring is indexed
// by a local per-viewport acquire counter.
//

#pragma once

#include <aurora/rdg/RenderViewport.h>
#include <aurora/rhi/SwapChain.h>
#include <aurora/rhi/Semaphore.h>

#include <vector>

namespace sky::aurora {

    class Device;

    class ClientViewport : public RenderViewport {
    public:
        explicit ClientViewport(Name name = Name{});
        ~ClientViewport() override = default;

        bool Init(Device *device, const SwapChain::Descriptor &desc);

        bool Begin() override;
        bool Acquire() override;
        void Release() override;

        Image       *GetBackbuffer() const override;
        PixelFormat  GetFormat() const override;
        Extent2D     GetExtent() const override;
        const Name  &GetName() const override;

        Semaphore *GetAcquireSemaphore() const override;
        Semaphore *GetRenderDoneSemaphore() const override;

    private:
        SwapChainPtr mSwapChain;
        Name         mName;

        std::vector<SemaphorePtr> mAcquireSemas;     // ring, indexed by mFrameSlot
        std::vector<SemaphorePtr> mRenderDoneSemas;  // ring, indexed by mFrameSlot

        uint32_t mFrameSlot   = 0;
        uint32_t mCurrentSlot = 0;
        uint32_t mImageIndex  = INVALID_INDEX;
        bool     mFrameValid  = false;
    };

} // namespace sky::aurora
