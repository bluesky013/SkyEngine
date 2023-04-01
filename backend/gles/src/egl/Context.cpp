//
// Created by Zach Lee on 2023/1/31.
//

#include <gles/egl/Context.h>
#include <core/logger/Logger.h>

static const char* TAG = "Context";

namespace sky::gles {

    Context::~Context()
    {
        if (context != EGL_NO_CONTEXT) {
            eglDestroyContext(display, context);
        }
    }

    void Context::PrintConfigs()
    {
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
        for (uint32_t i = 0;i < configs.size(); i++) {
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
        configs.resize(num);

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

    EGLConfig Context::QueryConfig(const Config &config) const
    {
        for (auto &cfg : configs) {
            EGLint alphaSize = 0;
            EGLint depthSize = 0;
            EGLint stencilSize = 0;
            EGLint samples = 0;

            eglGetConfigAttrib(display, cfg, EGL_ALPHA_SIZE,   &alphaSize);
            eglGetConfigAttrib(display, cfg, EGL_DEPTH_SIZE,   &depthSize);
            eglGetConfigAttrib(display, cfg, EGL_STENCIL_SIZE, &stencilSize);
            eglGetConfigAttrib(display, cfg, EGL_SAMPLES,      &samples);
            bool res = true;
            res &= alphaSize >= config.alpha;
            res &= depthSize >= config.depth;
            res &= stencilSize >= config.stencil;
            res &= samples >= config.sample;
            if (res) {
                return cfg;
            }
        }
        return EGL_NO_CONFIG_KHR;
    }

    void Context::MakeCurrent(const Surface &surface)
    {
        currentSurface = surface.GetSurface();
        eglMakeCurrent(display, currentSurface, currentSurface, context);
    }
}
