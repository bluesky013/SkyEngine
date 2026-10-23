//
// Created on 2026/04/02.
//

#pragma once

#include <array>
#include <aurora/rhi/SwapChain.h>
#include <memory>

namespace sky::aurora {

    class MetalDevice;
    class MetalImage;

    // Checked frames retain each drawable until GPU and presentation retirement.
    class MetalSwapChain : public SwapChain {
    public:
        static constexpr uint32_t IMAGE_COUNT = 3;

        explicit MetalSwapChain(MetalDevice &dev);
        ~MetalSwapChain() override;

        bool Init(const Descriptor &desc);

        Device                *GetDevice() const noexcept override;
        SurfaceQueryResult     QuerySurface() const noexcept override;
        SwapChainAcquireResult TryAcquireFrame(uint64_t timeoutNs) noexcept override;
        PresentationResult     TryPresentFrame(uint64_t serial, bool gpuComplete) noexcept override;
        FenceQueryResult       RetireFrame(uint64_t serial, bool abandon) noexcept override;
        PresentationResult     TryResize(uint32_t width, uint32_t height) noexcept override;
        PresentationResult     TryClose() noexcept override;

        uint32_t AcquireNextImage(Semaphore *signalSema, Fence *fence, uint64_t timeoutNs) override;
        void     Present(uint32_t imageIndex, uint32_t numWaitSemas, Semaphore *const *waitSemas) override;
        void     Resize(uint32_t width, uint32_t height) override;
        Image   *GetImage(uint32_t index) const override;
        uint32_t GetImageCount() const override
        {
            return IMAGE_COUNT;
        }
        PixelFormat GetFormat() const override
        {
            return format;
        }
        Extent2D GetExtent() const override
        {
            return extent;
        }
        SwapChainStatus GetStatus() const override;
        Extent2D        GetSurfaceSize() const override;

        void *GetLayer() const
        {
            return layer;
        }

    private:
        struct Slot {
            void                       *drawable     = nullptr;
            void                       *presentation = nullptr; // owned id<MTLCommandBuffer>
            std::unique_ptr<MetalImage> image;
            uint64_t                    serial = 0;
        };
        struct DrawableRequest;

        void ReleaseSlot(Slot &slot);
        bool HasFrames() const noexcept;

        MetalDevice                     &device;
        void                            *layer = nullptr; // CAMetalLayer
        std::array<Slot, IMAGE_COUNT>    slots;
        uint32_t                         acquireCursor = 0;
        PixelFormat                      format        = PixelFormat::BGRA8_UNORM;
        Extent2D                         extent        = {1, 1};
        std::unique_ptr<DrawableRequest> request;
        uint64_t                         presentationSerial = 0;
        uint64_t                         surfaceRevision    = 1;
        bool                             checkedProtocol    = false;
        bool                             directProtocol     = false;
        bool                             closed             = false;
    };

} // namespace sky::aurora
