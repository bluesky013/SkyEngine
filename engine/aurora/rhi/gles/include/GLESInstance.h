//
// Created on 2026/04/01.
//

#pragma once

#include <aurora/rhi/Instance.h>
#include <GLESForward.h>
#include <memory>

namespace sky {
    class DynamicModule;
}

namespace sky::aurora {

    class GLESDevice;

    class GLESInstance : public Instance::Impl {
    public:
        GLESInstance() = default;
        ~GLESInstance() override;

        bool Init(const Instance::Descriptor &desc) override;
        Device *CreateDevice() override;

        EGLDisplay GetEGLDisplay() const { return eglDisplay; }
        EGLConfig  GetEGLConfig() const { return eglConfig; }

    private:
        bool InitEGL();
        bool ChooseConfig();

        EGLDisplay eglDisplay = EGL_NO_DISPLAY;
        EGLConfig  eglConfig  = nullptr;

#ifdef WIN32
        std::unique_ptr<DynamicModule> eglModule;
#endif
    };

} // namespace sky::aurora
