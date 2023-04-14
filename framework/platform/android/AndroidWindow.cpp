//
// Created by Zach Lee on 2022/9/26.
//

#include "AndroidWindow.h"
#include <android/native_window.h>

namespace sky {

    bool AndroidWindow::Init(const Descriptor &desc)
    {
        descriptor = desc;
        if (descriptor.nativeHandle != nullptr) {
            winHandle = descriptor.nativeHandle;
        }

        descriptor.width = ANativeWindow_getWidth(static_cast<ANativeWindow*>(winHandle));
        descriptor.height = ANativeWindow_getHeight(static_cast<ANativeWindow*>(winHandle));
        return winHandle != nullptr;
    }

    void AndroidWindow::PollEvent(bool &quit)
    {
    }

    NativeWindow *NativeWindow::Create(const Descriptor &des)
    {
        NativeWindow *window = new AndroidWindow();
        if (!window->Init(des)) {
            delete window;
            window = nullptr;
        }
        return window;
    }
}
