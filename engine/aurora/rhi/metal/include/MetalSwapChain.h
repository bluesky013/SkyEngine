//
// Created on 2026/04/02.
//

#pragma once

#include <aurora/rhi/SwapChain.h>
#include <memory>

namespace sky::aurora {

    class MetalDevice;
    class MetalImage;

    class MetalSwapChain : public SwapChain {
    public:
        explicit MetalSwapChain(MetalDevice &dev);
        ~MetalSwapChain() override;

        bool Init(const Descriptor &desc);

        uint32_t    AcquireNextImage(Semaphore *signalSema, Fence *fence, uint64_t timeoutNs) override;
        void        Present(uint32_t imageIndex, uint32_t numWaitSemas, Semaphore *const *waitSemas) override;
        void        Resize(uint32_t width, uint32_t height) override;
        Image*      GetImage(uint32_t index) const override;
        uint32_t    GetImageCount() const override { return 1; }
        PixelFormat GetFormat() const override { return format; }
        Extent2D    GetExtent() const override { return extent; }

        void *GetLayer() const { return layer; }

    private:
        void ReleaseDrawable();

        MetalDevice                &device;
        void                       *layer            = nullptr;   // CAMetalLayer
        void                       *currentDrawable  = nullptr;   // id<CAMetalDrawable>
        std::unique_ptr<MetalImage> image;                        // wraps current drawable's texture
        PixelFormat                 format           = PixelFormat::BGRA8_UNORM;
        Extent2D                    extent           = {1, 1};
    };

} // namespace sky::aurora