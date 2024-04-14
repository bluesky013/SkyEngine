//
// Created by Zach Lee on 2021/11/11.
//


#include <framework/window/NativeWindow.h>

namespace sky {

    NativeWindow::NativeWindow()
    {
    }

    NativeWindow::~NativeWindow()
    {
    }

    void *NativeWindow::GetNativeHandle() const
    {
        return winHandle;
    }

} // namespace sky