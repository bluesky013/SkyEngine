//
// Created by Zach on 2023/1/31.
//

#pragma once

#include <memory>

#ifdef ANDROID
#include <EGL/egl.h>
#include <EGL/eglext.h>
#endif

#include <gles/Forward.h>

namespace sky::gles {

    class Surface {
    public:
        Surface() = default;
        virtual ~Surface();

        EGLSurface GetSurface() const { return surface; };

    protected:
        EGLSurface surface = EGL_NO_SURFACE;
    };
    using SurfacePtr = std::shared_ptr<Surface>;
}
