//
// Created on 2026/04/02.
//

#pragma once

#include <aurora/rhi/SwapChain.h>
#include <array>
#include <memory>

namespace sky::aurora {

    class MetalDevice;
    class MetalImage;

    // CAMetalLayer hands out drawables from an internal pool (capped by
    // maximumDrawableCount); AcquireNextImage rotates through IMAGE_COUNT
    // slots so backbuffers stay valid until presented, matching the
    // multi-image ring semantics of the other backends.
    class MetalSwapChain : public SwapChain {
    public:
        static constexpr uint32_t IMAGE_COUNT = 3;

        explicit MetalSwapChain(MetalDevice &dev);
        ~MetalSwapChain() override;

        bool Init(const Descriptor &desc);

        uint32_t    AcquireNextImage(Semaphore *signalSema, Fence *fence, uint64_t timeoutNs) override;
        void        Present(uint32_t imageIndex, uint32_t numWaitSemas, Semaphore *const *waitSemas) override;
        void        Resize(uint32_t width, uint32_t height) override;
        Image*      GetImage(uint32_t index) const override;
        uint32_t    GetImageCount() const override { return IMAGE_COUNT; }
        PixelFormat GetFormat() const override { return format; }
        Extent2D    GetExtent() const override { return extent; }
        SwapChainStatus GetStatus() const override;
        Extent2D    GetSurfaceSize() const override;

        void *GetLayer() const { return layer; }

    private:
        struct Slot {
            void                       *drawable = nullptr; // id<CAMetalDrawable>, owned
            std::unique_ptr<MetalImage> image;              // wraps the drawable's texture
        };

        void ReleaseSlot(Slot &slot);

        MetalDevice                 &device;
        void                        *layer = nullptr; // CAMetalLayer
        std::array<Slot, IMAGE_COUNT> slots;
        uint32_t                    acquireCursor = 0;
        PixelFormat                 format = PixelFormat::BGRA8_UNORM;
        Extent2D                    extent = {1, 1};
    };

} // namespace sky::aurora