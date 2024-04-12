//
// Created by Zach on 2023/1/30.
//

#include <gles/Instance.h>
#include <core/platform/Platform.h>
#include <core/util/DynamicModule.h>

#ifndef ANDROID
std::unique_ptr<sky::DynamicModule> g_Egl;
#include "egl/egl_win32.inl"
#else
#include "egl/egl_android.inl"
#endif

#include <gles/Device.h>
#include <gles/Ext.h>

PFN_FramebufferFetchBarrier FramebufferFetchBarrier;
PFN_MultiDrawArraysIndirectEXT MultiDrawArraysIndirectEXT;
PFN_MultiDrawElementsIndirectEXT MultiDrawElementsIndirectEXT;
PFN_RenderbufferStorageMultisampleEXT RenderbufferStorageMultisampleEXT;
PFN_FramebufferTexture2DMultisampleEXT FramebufferTexture2DMultisampleEXT;

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

    bool Instance::InitEGL()
    {
#ifndef ANDROID
        if (!g_Egl) {
            g_Egl = std::make_unique<DynamicModule>("libEGL.dll");
            g_Egl->Load();
        }
        if (g_Egl && g_Egl->IsLoaded()) {
            gladLoadEGL();
            InitConfig();
            return true;
        }
        return false;
#endif
        return true;
    }

    void Instance::InitConfig()
    {
        EGLint eglMajorVersion{0};
        EGLint eglMinorVersion{0};
        eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        eglInitialize(eglDisplay, &eglMajorVersion, &eglMinorVersion);
        eglBindAPI(EGL_OPENGL_ES_API);
    }

#define GLES_GET_API(name, key)   \
    if (name == nullptr) {      \
        name = reinterpret_cast<PFN_##name>(eglGetProcAddress(key)); \
    }

    bool Instance::Init(const Descriptor &desc)
    {
        if (!InitEGL()) {
            return false;
        }
        if (FramebufferFetchBarrier == nullptr) {
            FramebufferFetchBarrier = reinterpret_cast<PFN_FramebufferFetchBarrier>(eglGetProcAddress("glFramebufferFetchBarrierQCOM"));
        }
        if (FramebufferFetchBarrier == nullptr) {
            FramebufferFetchBarrier = reinterpret_cast<PFN_FramebufferFetchBarrier>(eglGetProcAddress("glFramebufferFetchBarrierEXT"));
        }

        if (MultiDrawArraysIndirectEXT == nullptr) {
            MultiDrawArraysIndirectEXT = reinterpret_cast<PFN_MultiDrawArraysIndirectEXT>(eglGetProcAddress("glMultiDrawArraysIndirectEXT"));
        }

        if (MultiDrawElementsIndirectEXT == nullptr) {
            MultiDrawElementsIndirectEXT = reinterpret_cast<PFN_MultiDrawElementsIndirectEXT>(eglGetProcAddress("glMultiDrawElementsIndirectEXT"));
        }

        GLES_GET_API(RenderbufferStorageMultisampleEXT, "glRenderbufferStorageMultisampleEXT");
        GLES_GET_API(FramebufferTexture2DMultisampleEXT, "glFramebufferTexture2DMultisampleEXT");
        return true;
    }
}
