//
// Created on 2026/04/01.
//

#include <GLESInstance.h>
#include <GLESDevice.h>
#include <GLESLoader.h>
#include <core/logger/Logger.h>
#include <core/platform/Platform.h>
#include <core/util/DynamicModule.h>

static const char *TAG = "AuroraGL";

#if SKY_PLATFORM_WINDOWS
static sky::DynamicModule *g_EglModule = nullptr;

static void *EglGetProcAddr(const char *name)
{
    return g_EglModule ? g_EglModule->GetAddress(name) : nullptr;
}
#endif

namespace sky::aurora {

    GLESInstance::~GLESInstance()
    {
        if (eglDisplay != EGL_NO_DISPLAY) {
            eglTerminate(eglDisplay);
            eglDisplay = EGL_NO_DISPLAY;
        }
#if SKY_PLATFORM_WINDOWS
        eglModule = nullptr;
#endif
    }

    bool GLESInstance::Init(const Instance::Descriptor &desc)
    {
        if (!InitEGL()) {
            return false;
        }
        if (!ChooseConfig()) {
            return false;
        }
        LOG_I(TAG, "GLES instance initialized");
        return true;
    }

    Device *GLESInstance::CreateDevice()
    {
        auto *device = new GLESDevice(*this);
        if (!device->Init()) {
            delete device;
            return nullptr;
        }
        return device;
    }

    bool GLESInstance::InitEGL()
    {
#if SKY_PLATFORM_WINDOWS
        eglModule = std::make_unique<DynamicModule>("libEGL");
        if (!eglModule->Load()) {
            LOG_E(TAG, "failed to load libEGL.dll (PowerVR emulation)");
            return false;
        }
        g_EglModule = eglModule.get();
        if (!LoadEGLFunctions(EglGetProcAddr)) {
            LOG_E(TAG, "failed to load EGL entry points");
            return false;
        }
#endif
        eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        if (eglDisplay == EGL_NO_DISPLAY) {
            LOG_E(TAG, "eglGetDisplay failed");
            return false;
        }

        EGLint major = 0;
        EGLint minor = 0;
        if (eglInitialize(eglDisplay, &major, &minor) != EGL_TRUE) {
            LOG_E(TAG, "eglInitialize failed: 0x%x", eglGetError());
            return false;
        }
        LOG_I(TAG, "EGL version: %d.%d", major, minor);

        eglBindAPI(EGL_OPENGL_ES_API);
        return true;
    }

    bool GLESInstance::ChooseConfig()
    {
        const EGLint attribs[] = {
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
            EGL_SURFACE_TYPE,    EGL_WINDOW_BIT | EGL_PBUFFER_BIT,
            EGL_RED_SIZE,        8,
            EGL_GREEN_SIZE,      8,
            EGL_BLUE_SIZE,       8,
            EGL_ALPHA_SIZE,      8,
            EGL_DEPTH_SIZE,      24,
            EGL_STENCIL_SIZE,    8,
            EGL_NONE
        };

        EGLint numConfigs = 0;
        if (eglChooseConfig(eglDisplay, attribs, &eglConfig, 1, &numConfigs) != EGL_TRUE || numConfigs == 0) {
            LOG_E(TAG, "eglChooseConfig failed: 0x%x", eglGetError());
            return false;
        }
        return true;
    }

} // namespace sky::aurora

extern "C" SKY_EXPORT sky::aurora::Instance::Impl *CreateInstance()
{
    return new sky::aurora::GLESInstance();
}
