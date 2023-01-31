//
// Created by Zach Lee on 2023/1/30.
//

#pragma once

#include <gles/Forward.h>
#include <gles/Config.h>
#include <gles/Surface.h>
#include <vector>

namespace sky::gles {

    class Context {
    public:
        Context() = default;
        ~Context();

        struct Descriptor {
            Config defaultConfig;
        };

        bool Init(const Descriptor &desc, EGLContext sharedContext = EGL_NO_CONTEXT);
        void MakeCurrent(const Surface &surface);

        EGLConfig QueryConfig(const Config &config) const;

        EGLContext GetNativeHandle() const;

    private:
        void PrintConfigs();

        EGLContext context{EGL_NO_CONTEXT};
        EGLDisplay display{EGL_NO_DISPLAY};
        EGLConfig config{EGL_NO_CONFIG_KHR};
        std::vector<EGLConfig> configs;
    };

}
