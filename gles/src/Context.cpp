//
// Created by Zach Lee on 2023/1/31.
//

#include <gles/Context.h>
#include <core/logger/Logger.h>

static const char* TAG = "Context";

namespace sky::gles {

    Context::~Context()
    {
        if (context != EGL_NO_CONTEXT) {
            eglDestroyContext(display, context);
        }
    }

    bool Context::Init(EGLContext sharedContext)
    {
        display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        EGLint defaultAttribs[]{
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT | EGL_PBUFFER_BIT,
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT_KHR,
            EGL_BLUE_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_RED_SIZE, 8,
            EGL_ALPHA_SIZE, 8,
            EGL_DEPTH_SIZE, 24,
            EGL_STENCIL_SIZE, 8,
            EGL_SAMPLE_BUFFERS, 0,
            EGL_SAMPLES, 0,
            EGL_NONE};
        EGLint num = 0;
        eglChooseConfig(display, defaultAttribs, nullptr, 0, &num);
        configs.resize(num);

        EGLint count = num;
        eglChooseConfig(display, defaultAttribs, configs.data(), count, &num);
        config = configs[0];


        /**
         * Special: EGL_CONFIG_CAVEAT
         * Special: EGL_COLOR_BUFFER_TYPE
         * Special: larger total number of color bits
         * Smaller EGL_BUFFER_SIZE.
         * Smaller EGL_SAMPLE_BUFFERS.
         * Smaller EGL_SAMPLES.
         * Smaller EGL_DEPTH_SIZE.
         * Smaller EGL_STENCIL_SIZE.
         * Smaller EGL_ALPHA_MASK_SIZE.
         * Special: EGL_NATIVE_VISUAL_TYPE
         * Smaller EGL_CONFIG_ID
         */
        for (uint32_t i = 0;i < num; i++) {
            EGLint alphaSize = 0;
            EGLint depthSize = 0;
            EGLint stencilSize = 0;
            EGLint samples = 0;

            eglGetConfigAttrib(display, configs[i], EGL_ALPHA_SIZE,   &alphaSize);
            eglGetConfigAttrib(display, configs[i], EGL_DEPTH_SIZE,   &depthSize);
            eglGetConfigAttrib(display, configs[i], EGL_STENCIL_SIZE, &stencilSize);
            eglGetConfigAttrib(display, configs[i], EGL_SAMPLES,      &samples);

            LOG_I(TAG, "egl config: alphaSize %d, depthSize %d, stencilSize %d, samples %d", alphaSize, depthSize, stencilSize, samples);
        }

        EGLint contextAttributes[] {
            EGL_CONTEXT_MAJOR_VERSION_KHR, 3,
            EGL_CONTEXT_MINOR_VERSION_KHR, 2,
            EGL_NONE};
        context = eglCreateContext(display, config, sharedContext, contextAttributes);
        return true;
    }

    EGLContext Context::GetNativeHandle() const
    {
        return context;
    }

}