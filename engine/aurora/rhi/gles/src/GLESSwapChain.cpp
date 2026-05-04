//
// Created on 2026/04/01.
//

#include <GLESSwapChain.h>
#include <GLESLoader.h>
#include <GLESDevice.h>
#include <GLESInstance.h>
#include <GLESFence.h>
#include <GLESSemaphore.h>
#include <core/logger/Logger.h>

static const char *TAG = "AuroraGL";

namespace sky::aurora {

    GLESSwapChain::GLESSwapChain(GLESDevice &dev)
        : device(dev)
    {
    }

    GLESSwapChain::~GLESSwapChain()
    {
        if (surface != EGL_NO_SURFACE) {
            eglDestroySurface(device.GetInstance().GetEGLDisplay(), surface);
            surface = EGL_NO_SURFACE;
        }
    }

    bool GLESSwapChain::Init(const Descriptor &desc)
    {
        if (desc.window == nullptr) {
            LOG_E(TAG, "swapchain requires a valid window handle");
            return false;
        }

        width  = desc.width;
        height = desc.height;
        format = desc.preferredFormat;

        auto &inst    = device.GetInstance();
        auto  display = inst.GetEGLDisplay();
        auto  config  = inst.GetEGLConfig();

        const EGLint surfaceAttribs[] = {
            EGL_NONE
        };

        surface = eglCreateWindowSurface(display, config,
            static_cast<EGLNativeWindowType>(desc.window), surfaceAttribs);
        if (surface == EGL_NO_SURFACE) {
            LOG_E(TAG, "eglCreateWindowSurface failed: 0x%x", eglGetError());
            return false;
        }

        if (desc.preferredMode == PresentMode::VSYNC) {
            eglSwapInterval(display, 1);
        } else {
            eglSwapInterval(display, 0);
        }

        // Query actual size
        EGLint w = 0, h = 0;
        eglQuerySurface(display, surface, EGL_WIDTH, &w);
        eglQuerySurface(display, surface, EGL_HEIGHT, &h);
        if (w > 0 && h > 0) {
            width  = static_cast<uint32_t>(w);
            height = static_cast<uint32_t>(h);
        }

        LOG_I(TAG, "swapchain created: %ux%u", width, height);
        return true;
    }

    uint32_t GLESSwapChain::AcquireNextImage(Semaphore *signalSema, Fence *fence, uint64_t /*timeoutNs*/)
    {
        // GLES default-FBO model has a single backbuffer; immediately signal.
        if (signalSema != nullptr) {
            auto *sema = static_cast<GLESSemaphore *>(signalSema);
            const uint64_t v = (sema->GetType() == SemaphoreType::TIMELINE)
                                   ? 1
                                   : (sema->GetCurrentValue() + 1);
            sema->Signal(v);
        }
        if (fence != nullptr) {
            static_cast<GLESFence *>(fence)->SignalFromQueue();
        }
        return 0;
    }

    void GLESSwapChain::Present(uint32_t /*imageIndex*/, uint32_t numWaitSemas, Semaphore *const *waitSemas)
    {
        // CPU-side block on wait semaphores (single-threaded GLES emulation).
        for (uint32_t i = 0; i < numWaitSemas; ++i) {
            auto *sema = static_cast<GLESSemaphore *>(waitSemas[i]);
            if (sema == nullptr) continue;
            const uint64_t target = (sema->GetType() == SemaphoreType::TIMELINE)
                                        ? sema->GetCurrentValue()
                                        : sema->GetCurrentValue();
            (void)sema->Wait(target, UINT64_MAX);
        }
        auto display = device.GetInstance().GetEGLDisplay();
        if (eglSwapBuffers(display, surface) != EGL_TRUE) {
            LOG_E(TAG, "eglSwapBuffers failed: 0x%x", eglGetError());
        }
    }

    void GLESSwapChain::Resize(uint32_t w, uint32_t h)
    {
        width  = w;
        height = h;
        // EGL window surfaces auto-resize; just update cached dimensions
        auto display = device.GetInstance().GetEGLDisplay();
        EGLint ew = 0, eh = 0;
        eglQuerySurface(display, surface, EGL_WIDTH, &ew);
        eglQuerySurface(display, surface, EGL_HEIGHT, &eh);
        if (ew > 0 && eh > 0) {
            width  = static_cast<uint32_t>(ew);
            height = static_cast<uint32_t>(eh);
        }
    }

} // namespace sky::aurora
