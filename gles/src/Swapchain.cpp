//
// Created by Zach Lee on 2023/1/31.
//

#include <gles/Swapchain.h>
#include <gles/Device.h>
#include <gles/Core.h>

namespace sky::gles {

    bool WindowSurface::Init(EGLConfig config, void *window)
    {
        auto display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        surface = eglCreatePlatformWindowSurface(display, config, window, nullptr);
        EGLint width;
        EGLint height;
        eglQuerySurface(display, surface, EGL_WIDTH, &width);
        eglQuerySurface(display, surface, EGL_HEIGHT, &height);
        extent.width = static_cast<uint32_t>(width);
        extent.height = static_cast<uint32_t>(height);

        return surface != EGL_NO_SURFACE;
    }

    SwapChain::~SwapChain()
    {
    }

    bool SwapChain::Init(const Descriptor &desc)
    {
        Config cfg = {};
        auto config = device.GetMainContext()->QueryConfig(cfg);
        surface = std::make_shared<WindowSurface>();
        if (!surface->Init(config, desc.window)) {
            return false;
        }
        auto &ext = surface->GetExtent();

        color = std::make_shared<Image>(device);
        color->imageDesc.extent = {ext.width, ext.height, 1};
        color->imageDesc.usage = rhi::ImageUsageFlagBit::RENDER_TARGET;
        color->imageDesc.format = rhi::PixelFormat::RGBA8_UNORM;
        color->surface = surface;

        depth = std::make_shared<Image>(device);
        depth->imageDesc.extent = {ext.width, ext.height, 1};
        depth->imageDesc.usage = rhi::ImageUsageFlagBit::DEPTH_STENCIL;
        depth->imageDesc.format = rhi::PixelFormat::D24_S8;
        depth->surface = surface;

        return true;
    }

    rhi::ImagePtr SwapChain::AcquireNextImage() const
    {
        return color;
    }
}
