//
// Created by Zach Lee on 2021/11/11.
//

#include "framework/window/NativeWindow.h"
#include "core/util/DynamicModule.h"
#include "PlatformImpl.h"

namespace sky {

    NativeWindow::NativeWindow() : impl(nullptr)
    {
    }

    NativeWindow::~NativeWindow()
    {
        if (impl != nullptr) {
            delete impl;
        }
    }

    bool NativeWindow::Init(const Descriptor& des)
    {
        impl = Impl::Create(des);
        return impl != nullptr;
    }

    NativeWindow* NativeWindow::Create(const Descriptor& des)
    {
        auto window = new NativeWindow();
        if (!window->Init(des)) {
            delete window;
            window = nullptr;
        }
        return window;
    }

    NativeWindow::Impl* NativeWindow::Impl::Create(const Descriptor& des)
    {
        return PlatformImpl::Get()->CreateWindow(des);
    }

    void* NativeWindow::GetNativeHandle() const
    {
        return impl->GetNativeHandle();
    }

}