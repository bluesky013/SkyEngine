//
// Created by Zach on 2023/1/30.
//

#include <gles/Instance.h>
#include <gles/Device.h>
#include <core/platform/Platform.h>
#include <core/util/DynamicModule.h>
std::unique_ptr<sky::DynamicModule> g_Gles;
std::unique_ptr<sky::DynamicModule> g_Egl;

#include "egl.inl"

namespace sky::gles {

    rhi::Device *Instance::CreateDevice(const rhi::Device::Descriptor &desc)
    {
        auto device = std::make_unique<Device>();
        if (device->Init(desc)) {
            return device.release();
        }
        return nullptr;
    }

    Instance::~Instance()
    {
        eglTerminate(eglDisplay);
    }

    bool Instance::InitGL()
    {
        if (!g_Gles) {
            g_Gles = std::make_unique<DynamicModule>("libGLESV2.dll");
            g_Gles->Load();
        }
        if (g_Gles && g_Gles->IsLoaded()) {
            gladLoadGLES2Loader(gladLoadProcAddress);
            return true;
        }

        return false;
    }

    bool Instance::InitEGL()
    {
        if (!g_Egl) {
            g_Egl = std::make_unique<DynamicModule>("libEGL.dll");
            g_Egl->Load();
        }
        if (g_Egl && g_Egl->IsLoaded()) {
            gladLoadEGL();
            return true;
        }
        return false;
    }

    void Instance::InitConfig()
    {
        EGLint eglMajorVersion{0};
        EGLint eglMinorVersion{0};
        eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        eglInitialize(eglDisplay, &eglMajorVersion, &eglMinorVersion);
        eglBindAPI(EGL_OPENGL_ES_API);
    }

    bool Instance::Init(const Descriptor &desc)
    {
        if (!InitEGL()) {
            return false;
        }
        if (!InitGL()) {
            return false;
        }
        InitConfig();
        return true;
    }
}

extern "C" SKY_EXPORT sky::rhi::Instance *CreateInstance()
{
    return new sky::gles::Instance();
}
