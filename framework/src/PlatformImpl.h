//
// Created by Zach Lee on 2021/12/3.
//

#pragma once
#include <memory>
#include <core/util/DynamicModule.h>
#include <framework/Application.h>
#include <framework/window/NativeWindow.h>

namespace sky {

    class PlatformImpl {
    public:
        static PlatformImpl* Get();

        NativeWindow::Impl* CreateWindow(const NativeWindow::Descriptor& des);

        Application::Impl* CreateApplication();

    private:
        PlatformImpl() = default;
        ~PlatformImpl() = default;

        using WindowCreateFn = NativeWindow::Impl*(*)(const sky::NativeWindow::Descriptor& des);
        using ApplicationCreateFn = Application::Impl*(*)();

        std::unique_ptr<DynamicModule> module;
        WindowCreateFn windowCreateFn = nullptr;
        ApplicationCreateFn applicationCreateFn = nullptr;
    };
}
