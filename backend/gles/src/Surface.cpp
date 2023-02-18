//
// Created by Zach on 2023/1/31.
//

#include <gles/Surface.h>

namespace sky::gles {

    Surface::~Surface()
    {
        if (surface != EGL_NO_CONFIG_KHR) {
            eglDestroySurface(eglGetDisplay(EGL_DEFAULT_DISPLAY), surface);
        }
    }

}

