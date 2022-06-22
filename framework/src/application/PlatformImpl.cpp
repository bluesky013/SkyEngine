//
// Created by Zach Lee on 2021/12/3.
//

#include "PlatformImpl.h"

namespace sky {

    PlatformImpl* PlatformImpl::Get()
    {
        static PlatformImpl* instance = nullptr;
        if (instance == nullptr) {
            instance = new PlatformImpl();

            instance->module = std::make_unique<DynamicModule>("FrameworkImpl");
            instance->module->Load();
            instance->attachEnvFn = instance->module->GetAddress<AttachEnvFn>("AttachEnvironment");
            instance->windowCreateFn = instance->module->GetAddress<WindowCreateFn>("CreateNativeWindow");
            instance->applicationCreateFn = instance->module->GetAddress<ApplicationCreateFn>("CreateApplication");

            instance->attachEnvFn(Environment::Get());
        }
        return instance;
    }

    NativeWindowImpl* PlatformImpl::CreateWindow(const NativeWindow::Descriptor& des)
    {
        return windowCreateFn(des);
    }

    ApplicationImpl* PlatformImpl::CreateApplication()
    {
        return applicationCreateFn();
    }
}