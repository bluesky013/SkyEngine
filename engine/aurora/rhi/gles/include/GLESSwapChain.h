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

        void Present();
        void Resize(uint32_t width, uint32_t height);

        EGLSurface GetEGLSurface() const { return surface; }
        PixelFormat GetFormat() const { return format; }
        uint32_t GetWidth() const { return width; }
        uint32_t GetHeight() const { return height; }

    private:
        GLESDevice &device;
        EGLSurface  surface = EGL_NO_SURFACE;
        PixelFormat format  = PixelFormat::RGBA8_UNORM;
        uint32_t    width   = 1;
        uint32_t    height  = 1;
    };

} // namespace sky::aurora
