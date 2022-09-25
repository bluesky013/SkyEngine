//
// Created by Zach Lee on 2022/9/25.
//

#include "Win32Window.h"
#include <SDL2/SDL_syswm.h>

namespace sky {

    bool Win32Window::Init(const Descriptor &desc)
    {
        if (!SDLWindow::Init(desc)) {
            return false;
        }

        SDL_SysWMinfo wmInfo;
        SDL_VERSION(&wmInfo.version);
        SDL_GetWindowWMInfo(window, &wmInfo);
        winHandle = reinterpret_cast<void *>(wmInfo.info.win.window);
        return winHandle != nullptr;
    }

    NativeWindow *NativeWindow::Create(const Descriptor &des)
    {
        NativeWindow *window = new Win32Window();
        if (!window->Init(des)) {
            delete window;
            window = nullptr;
        }
        return window;
    }
}