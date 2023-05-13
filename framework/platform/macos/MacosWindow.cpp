//
// Created by Zach Lee on 2022/9/25.
//

#include "MacosWindow.h"
#include <SDL2/SDL_syswm.h>

namespace sky {

    bool MacosWindow::Init(const Descriptor &desc)
    {
        if (!SDLWindow::Init(desc)) {
            return false;
        }

        UpdateWindow();
        return winHandle != nullptr;
    }

    void MacosWindow::UpdateWindow()
    {
        SDL_SysWMinfo wmInfo;
        SDL_VERSION(&wmInfo.version);
        SDL_GetWindowWMInfo(window, &wmInfo);
        winHandle = reinterpret_cast<void *>(wmInfo.info.cocoa.window);
    }

    NativeWindow *NativeWindow::Create(const Descriptor &des)
    {
        NativeWindow *window = new MacosWindow();
        if (!window->Init(des)) {
            delete window;
            window = nullptr;
        }
        return window;
    }
}