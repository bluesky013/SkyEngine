//
// Created by Zach Lee on 2021/11/11.
//

#include "application/window/NativeWindow.h"

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

    void* NativeWindow::GetNativeHandle() const
    {
        return impl->GetNativeHandle();
    }

}