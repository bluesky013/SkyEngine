//
// Created by Zach Lee on 2023/1/31.
//

#include <gles/egl/Context.h>
#include <core/logger/Logger.h>
#include <core/platform/Platform.h>

static const char* TAG = "Context";

namespace sky::gles {

    Context::~Context()
    {
        if (context != EGL_NO_CONTEXT) {
            eglDestroyContext(display, context);
        }
    }

    bool Context::Init(const Descriptor &desc)
    {
        display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        EGLint defaultAttribs[]{
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT | EGL_PBUFFER_BIT,
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT_KHR,
            EGL_BLUE_SIZE, desc.defaultConfig.rgb,
            EGL_GREEN_SIZE, desc.defaultConfig.rgb,
            EGL_RED_SIZE, desc.defaultConfig.rgb,
            EGL_ALPHA_SIZE, desc.defaultConfig.alpha,
            EGL_DEPTH_SIZE, desc.defaultConfig.depth,
            EGL_STENCIL_SIZE, desc.defaultConfig.stencil,
            EGL_SAMPLE_BUFFERS, 0,
            EGL_SAMPLES, desc.defaultConfig.sample,
            EGL_NONE};
        EGLint num = 0;
        eglChooseConfig(display, defaultAttribs, nullptr, 0, &num);
        std::vector<EGLConfig> configs(num);

        EGLint count = num;
        eglChooseConfig(display, defaultAttribs, configs.data(), count, &num);
        config = configs[0];

        EGLint contextAttributes[] {
            EGL_CONTEXT_MAJOR_VERSION_KHR, 3,
            EGL_CONTEXT_MINOR_VERSION_KHR, 2,
            EGL_NONE};
        context = eglCreateContext(display, config, desc.sharedContext, contextAttributes);

        pBuffer = std::make_unique<PBuffer>();
        pBuffer->Init(config);
        MakeCurrent(*pBuffer);
        return true;
    }

    EGLConfig Context::GetConfig() const
    {
        return config;
    }

    void Context::MakeCurrent(const Surface &surface)
    {
        auto handle = surface.GetSurface();
        if (currentSurface != handle) {
            currentSurface = handle;
            eglMakeCurrent(display, currentSurface, currentSurface, context);
            SKY_ASSERT(eglGetError() == EGL_SUCCESS);
        }
    }
}
