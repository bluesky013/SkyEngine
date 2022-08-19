//
// Created by Zach Lee on 2021/11/11.
//

#include <core/util/DynamicModule.h>
#include <framework/window/NativeWindow.h>
#include <framework/window/IWindowEvent.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>

namespace sky {

    NativeWindow::NativeWindow()
    {
    }

    NativeWindow::~NativeWindow()
    {
        SDL_DestroyWindow(window);
    }

    bool NativeWindow::Init(const Descriptor& des)
    {
        window = SDL_CreateWindow(des.titleName.c_str(), SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
            des.width, des.height,SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE | SDL_WINDOW_MOUSE_CAPTURE | SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_MOUSE_FOCUS);

        SDL_SysWMinfo wmInfo;
        SDL_VERSION(&wmInfo.version);
        SDL_GetWindowWMInfo(window, &wmInfo);
#if defined(_WIN32)
        winHandle = reinterpret_cast<void*>(wmInfo.info.win.window);
#else defined(__APPLE__)
        winHandle = reinterpret_cast<void*>(wmInfo.info.cocoa.window);
#endif
        return winHandle != nullptr;
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
        return winHandle;
    }

    void NativeWindow::PollEvent(bool &quit)
    {
        SDL_Event sdlEvent;
        int       cnt = 0;
        while ((cnt = SDL_PollEvent(&sdlEvent)) != 0) {
            Dispatch(sdlEvent, quit);
        }
    }

    void NativeWindow::Dispatch(const SDL_WindowEvent &event)
    {
        switch (event.event) {
            case SDL_WINDOWEVENT_SHOWN: {
                break;
            }
            case SDL_WINDOWEVENT_RESTORED: {
                break;
            }
            case SDL_WINDOWEVENT_SIZE_CHANGED: {
                Event<IWindowEvent>::BroadCast(winHandle, &IWindowEvent::OnWindowResize, static_cast<uint32_t>(event.data1), static_cast<uint32_t>(event.data2));
                break;
            }
            case SDL_WINDOWEVENT_RESIZED: {
                break;
            }
            case SDL_WINDOWEVENT_HIDDEN: {
                break;
            }
            case SDL_WINDOWEVENT_MINIMIZED: {
                break;
            }
            case SDL_WINDOWEVENT_ENTER: {
//                SDL_CaptureMouse(SDL_TRUE);
                break;
            }
            case SDL_WINDOWEVENT_CLOSE: {
                break;
            }
        }
    }

    void NativeWindow::Dispatch(const SDL_Event &sdlEvent, bool &quit) {
        switch (sdlEvent.type) {
            case SDL_QUIT: {
                quit = true;
                break;
            }
            case SDL_WINDOWEVENT: {
                Dispatch(sdlEvent.window);
                break;
            }
            case SDL_MOUSEBUTTONDOWN: {
                int width  = 0;
                int height = 0;
                SDL_GetWindowSize(window, &width, &height);
                const SDL_MouseButtonEvent &event = sdlEvent.button;
                break;
            }
            case SDL_MOUSEBUTTONUP: {
                const SDL_MouseButtonEvent &event = sdlEvent.button;
                break;
            }
            case SDL_MOUSEMOTION: {
                const SDL_MouseMotionEvent &event = sdlEvent.motion;
                break;
            }
            case SDL_MOUSEWHEEL: {
                const SDL_MouseWheelEvent &event = sdlEvent.wheel;
                break;
            }
            case SDL_KEYDOWN: {
                const SDL_KeyboardEvent &event = sdlEvent.key;
                break;
            }
            case SDL_KEYUP: {
                const SDL_KeyboardEvent &event = sdlEvent.key;
                break;
            }
            default:
                break;
        }
    }

}