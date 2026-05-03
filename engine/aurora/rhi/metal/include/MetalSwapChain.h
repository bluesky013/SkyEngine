//
// Created on 2026/04/02.
//

#pragma once

#include <aurora/rhi/SwapChain.h>

namespace sky::aurora {

    class MetalDevice;

    class MetalSwapChain : public SwapChain {
    public:
        explicit MetalSwapChain(MetalDevice &dev);
        ~MetalSwapChain() override;

        bool Init(const Descriptor &desc);

        // SwapChain interface — TODO: full implementation in aurora-queue-submit-present Metal phase
        uint32_t    AcquireNextImage(Semaphore *signalSema, Fence *fence, uint64_t timeoutNs) override { return 0; }
        void        Present(uint32_t imageIndex, uint32_t numWaitSemas, Semaphore *const *waitSemas) override {}
        void        Resize(uint32_t width, uint32_t height) override { extent = {width, height}; }
        Image*      GetImage(uint32_t index) const override { return nullptr; }
        uint32_t    GetImageCount() const override { return 1; }
        PixelFormat GetFormat() const override { return format; }
        Extent2D    GetExtent() const override { return extent; }

        void *GetLayer() const { return layer; }

    private:
        MetalDevice &device;
        void        *layer  = nullptr;
        PixelFormat  format = PixelFormat::BGRA8_UNORM;
        Extent2D     extent = {1, 1};
    };

} // namespace sky::aurora