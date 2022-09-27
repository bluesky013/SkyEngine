//
// Created by Zach Lee on 2022/9/26.
//

#include "AndroidWindow.h"

namespace sky {

    bool AndroidWindow::Init(const Descriptor &desc)
    {
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
