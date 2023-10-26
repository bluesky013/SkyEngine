//
// Created by Zach on 2023/1/31.
//

#include <gles/egl/Surface.h>
#include <gles/egl/WindowSurface.h>
#include <gles/egl/PBuffer.h>

namespace sky::gles {

    Surface::~Surface()
    {
        if (surface != EGL_NO_CONFIG_KHR) {
            eglDestroySurface(eglGetDisplay(EGL_DEFAULT_DISPLAY), surface);
        }
    }

    bool PBuffer::Init(EGLConfig config)
    {
        EGLint attributes[] = {
            EGL_WIDTH, 1,
            EGL_HEIGHT, 1,
            EGL_NONE
        };
        surface = eglCreatePbufferSurface(eglGetDisplay(EGL_DEFAULT_DISPLAY), config, attributes);
        return surface != EGL_NO_SURFACE;
    }

    bool WindowSurface::Init(EGLConfig config, void *window)
    {
        auto display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
#ifdef ANDROID
        surface = eglCreateWindowSurface(display, config, static_cast<EGLNativeWindowType>(window), nullptr);
#else
        surface = eglCreatePlatformWindowSurface(display, config, window, nullptr);
#endif
        EGLint width;
        EGLint height;
        eglQuerySurface(display, surface, EGL_WIDTH, &width);
        eglQuerySurface(display, surface, EGL_HEIGHT, &height);
        extent.width = static_cast<uint32_t>(width);
        extent.height = static_cast<uint32_t>(height);

        return surface != EGL_NO_SURFACE;
    }

}

