//
// Created by Zach Lee on 2021/12/3.
//

#pragma once
#include <memory>
#include <core/util/DynamicModule.h>
#include <framework/application/Application.h>
#include <framework/window/NativeWindow.h>

namespace sky {

    class PlatformImpl {
    public:
        static PlatformImpl* Get();

        NativeWindowImpl* CreateWindow(const NativeWindow::Descriptor& des);

        ApplicationImpl* CreateApplication();

    private:
        PlatformImpl() = default;
        ~PlatformImpl() = default;

        using WindowCreateFn = NativeWindowImpl*(*)(const sky::NativeWindow::Descriptor& des);
        using ApplicationCreateFn = ApplicationImpl*(*)();

        std::unique_ptr<DynamicModule> module;
        WindowCreateFn windowCreateFn = nullptr;
        ApplicationCreateFn applicationCreateFn = nullptr;
    };
}
