//
// Created by Zach Lee on 2023/1/30.
//

#pragma once

#include <rhi/Core.h>
#include <gles/Forward.h>
#include <gles/egl/Config.h>
#include <gles/egl/Surface.h>
#include <gles/egl/PBuffer.h>
#include <vector>

namespace sky::gles {

    class Context {
    public:
        Context() = default;
        ~Context();

        struct Descriptor {
            Config defaultConfig;
            EGLContext sharedContext = EGL_NO_CONTEXT;
        };

        bool Init(const Descriptor &desc);
        void MakeCurrent(const Surface &surface);

        EGLConfig GetConfig() const;
        EGLContext GetNativeHandle() const { return context; }
        EGLSurface GetReadSurface() const { return readSurface; }
        EGLSurface GetDrawSurface() const { return drawSurface; }
        EGLDisplay GetDisplay() const { return display; }

        rhi::PipelineState gfxState;
    private:
        void PrintConfigs();
        std::unique_ptr<PBuffer> pBuffer;
        EGLContext context = EGL_NO_CONTEXT;
        EGLDisplay display = EGL_NO_DISPLAY;
        EGLSurface readSurface = EGL_NO_SURFACE;
        EGLSurface drawSurface = EGL_NO_SURFACE;
        EGLConfig config = EGL_NO_CONFIG_KHR;
    };

}
