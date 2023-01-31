//
// Created by Zach Lee on 2023/1/31.
//

#include <gles/Swapchain.h>
#include <gles/Device.h>

namespace sky::gles {

    SwapChain::~SwapChain()
    {
    }

    bool SwapChain::Init(const Descriptor &desc)
    {
        descriptor = desc;
        Config cfg = {};
        auto config = device.GetMainContext()->QueryConfig(cfg);
        surface = eglCreatePlatformWindowSurface(eglGetDisplay(EGL_DEFAULT_DISPLAY), config, desc.window, nullptr);
        return surface != EGL_NO_SURFACE;
    }
}
