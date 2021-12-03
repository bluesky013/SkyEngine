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
            instance->windowCreateFn = instance->module->GetAddress<WindowCreateFn>("CreateWindow");
            instance->applicationCreateFn = instance->module->GetAddress<ApplicationCreateFn>("CreateApplication");
        }
        return instance;
    }

    NativeWindow::Impl* PlatformImpl::CreateWindow(const NativeWindow::Descriptor& des)
    {
        return windowCreateFn(des);
    }

    Application::Impl* PlatformImpl::CreateApplication()
    {
        return applicationCreateFn();
    }
}