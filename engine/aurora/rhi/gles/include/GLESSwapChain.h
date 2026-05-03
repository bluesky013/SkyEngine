//
// Created on 2026/04/01.
//

#pragma once

#include <aurora/rhi/SwapChain.h>
#include <GLESForward.h>

namespace sky::aurora {

    class GLESDevice;
    class GLESImage;

    class GLESSwapChain : public SwapChain {
    public:
        explicit GLESSwapChain(GLESDevice &dev);
        ~GLESSwapChain() override;

        bool Init(const Descriptor &desc);

        // SwapChain interface — TODO: full implementation in aurora-queue-submit-present GLES phase
        uint32_t    AcquireNextImage(Semaphore *signalSema, Fence *fence, uint64_t timeoutNs) override { return 0; }
        void        Present(uint32_t imageIndex, uint32_t numWaitSemas, Semaphore *const *waitSemas) override {}
        void        Resize(uint32_t w, uint32_t h) override;
        Image*      GetImage(uint32_t index) const override { return nullptr; }
        uint32_t    GetImageCount() const override { return 1; }
        PixelFormat GetFormat() const override { return format; }
        Extent2D    GetExtent() const override { return {width, height}; }

        EGLSurface GetEGLSurface() const { return surface; }

    private:
        GLESDevice &device;
        EGLSurface  surface = EGL_NO_SURFACE;
        PixelFormat format  = PixelFormat::RGBA8_UNORM;
        uint32_t    width   = 1;
        uint32_t    height  = 1;
    };

} // namespace sky::aurora
