//
// Created by Zach Lee on 2023/1/30.
//

#pragma once

#include <gles/Forward.h>
#include <vector>

namespace sky::gles {

    class Context {
    public:
        Context() = default;
        ~Context();

        bool Init(EGLContext sharedContext = EGL_NO_CONTEXT);

        EGLContext GetNativeHandle() const;

    private:
        EGLContext context{EGL_NO_CONTEXT};
        EGLDisplay display{EGL_NO_DISPLAY};
        EGLConfig config{EGL_NO_CONFIG_KHR};
        std::vector<EGLConfig> configs;
    };

}