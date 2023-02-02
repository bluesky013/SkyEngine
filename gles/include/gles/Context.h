//
// Created by Zach Lee on 2023/1/30.
//

#pragma once

#include <gles/Forward.h>
#include <gles/Config.h>
#include <gles/Surface.h>
#include <gles/PBuffer.h>
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

        EGLConfig QueryConfig(const Config &config) const;
        EGLContext GetNativeHandle() const { return context; }
        EGLSurface GetCurrentSurface() const { return currentSurface; }

    private:
        void PrintConfigs();
        std::unique_ptr<PBuffer> pBuffer;
        EGLContext context = EGL_NO_CONTEXT;
        EGLDisplay display = EGL_NO_DISPLAY;
        EGLSurface currentSurface = EGL_NO_SURFACE;
        EGLConfig config = EGL_NO_CONFIG_KHR;
        std::vector<EGLConfig> configs;
    };

}
