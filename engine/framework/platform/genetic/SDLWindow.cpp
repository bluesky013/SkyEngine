//
// Created by Zach Lee on 2022/9/25.
//

#include "SDLWindow.h"
#include <framework/window/IWindowEvent.h>
#include <framework/window/NativeWindow.h>
#include <framework/window/NativeWindowManager.h>

namespace sky {

    SDLWindow::~SDLWindow()
    {
        SDL_DestroyWindow(window);
    }

    bool SDLWindow::Init(const Descriptor &desc)
    {
        descriptor = desc;
        window = SDL_CreateWindow(desc.titleName.c_str(), SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, desc.width, desc.height,
                                  SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE | SDL_WINDOW_MOUSE_CAPTURE |
                                      SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_MOUSE_FOCUS | SDL_WINDOW_VULKAN);

        if (window != nullptr) {
            winID = SDL_GetWindowID(window);
            NativeWindowManager::Get()->Register(this);
        }
        return window != nullptr;
    }

    void SDLWindow::Dispatch(const SDL_WindowEvent &event) {
        switch (event.event) {
            case SDL_WINDOWEVENT_SHOWN:
            case SDL_WINDOWEVENT_RESTORED:
                break;
//            case SDL_WINDOWEVENT_SIZE_CHANGED:
            case SDL_WINDOWEVENT_RESIZED: {
                UpdateWindow();
                WindowResizeEvent mEvent = {};
                mEvent.winID = winID;
                mEvent.width = (uint32_t)(event.data1 * scale);
                mEvent.height = (uint32_t)(event.data2 * scale);
                Event<IWindowEvent>::BroadCast(this, &IWindowEvent::OnWindowResize, mEvent);
                break;
            }
            case SDL_WINDOWEVENT_FOCUS_GAINED:
            case SDL_WINDOWEVENT_FOCUS_LOST: {
                Event<IWindowEvent>::BroadCast(this, &IWindowEvent::OnFocusChanged, event.event == SDL_WINDOWEVENT_FOCUS_GAINED);
                break;
            }
            case SDL_WINDOWEVENT_HIDDEN:
            case SDL_WINDOWEVENT_MINIMIZED:
            case SDL_WINDOWEVENT_ENTER:
            case SDL_WINDOWEVENT_CLOSE: {
                break;
            }
        }
    }
}
