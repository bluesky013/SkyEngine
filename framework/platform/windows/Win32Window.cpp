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

//bool NativeWindow::Init(const Descriptor &des)
//{
//    window = SDL_CreateWindow(des.titleName.c_str(), SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, des.width, des.height,
//                              SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE | SDL_WINDOW_MOUSE_CAPTURE |
//                                  SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_MOUSE_FOCUS);
//
//    SDL_SysWMinfo wmInfo;
//    SDL_VERSION(&wmInfo.version);
//    SDL_GetWindowWMInfo(window, &wmInfo);
//#if defined(_WIN32)
//    winHandle = reinterpret_cast<void *>(wmInfo.info.win.window);
//#else defined(__APPLE__)
//    winHandle = reinterpret_cast<void *>(wmInfo.info.cocoa.window);
//#endif
//    return winHandle != nullptr;
//}