//
// Created by Zach on 2023/1/31.
//

#include <gles/PBuffer.h>

namespace sky::gles {

    void PBuffer::Init(EGLConfig config)
    {
        EGLint attributes[] = {
            EGL_WIDTH, 1,
            EGL_HEIGHT, 1,
            EGL_NONE
        };
        surface = eglCreatePbufferSurface(eglGetDisplay(EGL_DEFAULT_DISPLAY), config, attributes);
    }

}
