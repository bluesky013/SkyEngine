//
// Created by Zach Lee on 2021/11/11.
//


#include <framework/window/NativeWindow.h>
#include <core/platform/Platform.h>
#include <framework/window/NativeWindowManager.h>

namespace sky {

    NativeWindow::NativeWindow()
    {
    }

    NativeWindow::~NativeWindow()
    {
        NativeWindowManager::Get()->UnRegister(this);
    }

    void *NativeWindow::GetNativeHandle() const
    {
        return winHandle;
    }

    void NativeWindow::SetID(WindowID id)
    {
        SKY_ASSERT(winID == INVALID_WIN_ID);
        winID = id;
    }

} // namespace sky