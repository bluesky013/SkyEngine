//
// Created by Zach Lee on 2022/9/25.
//

#include "SDLWindow.h"
#include <framework/window/IWindowEvent.h>
#include <framework/window/NativeWindow.h>

namespace sky {

    SDLWindow::~SDLWindow()
    {
        SDL_DestroyWindow(window);
    }

    bool SDLWindow::Init(const Descriptor &desc)
    {
        window = SDL_CreateWindow(desc.titleName.c_str(), SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, desc.width, desc.height,
                                  SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE | SDL_WINDOW_MOUSE_CAPTURE |
                                      SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_MOUSE_FOCUS | SDL_WINDOW_VULKAN);
        return window != nullptr;
    }

    void SDLWindow::PollEvent(bool &quit)
    {
        SDL_Event sdlEvent;
        int       cnt = 0;
        while ((cnt = SDL_PollEvent(&sdlEvent)) != 0) {
            Dispatch(sdlEvent, quit);
        }
    }

    void SDLWindow::Dispatch(const SDL_WindowEvent &event)
    {
        switch (event.event) {
        case SDL_WINDOWEVENT_SHOWN: {
            break;
        }
        case SDL_WINDOWEVENT_RESTORED: {
            break;
        }
        case SDL_WINDOWEVENT_SIZE_CHANGED: {
            Event<IWindowEvent>::BroadCast(winHandle, &IWindowEvent::OnWindowResize, static_cast<uint32_t>(event.data1),
                                           static_cast<uint32_t>(event.data2));
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

    void SDLWindow::Dispatch(const SDL_Event &sdlEvent, bool &quit)
    {
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
            const SDL_MouseButtonEvent &event = sdlEvent.button;
            Event<IWindowEvent>::BroadCast(winHandle, &IWindowEvent::OnMouseButtonDown, event.button);
            break;
        }
        case SDL_MOUSEBUTTONUP: {
            const SDL_MouseButtonEvent &event = sdlEvent.button;
            Event<IWindowEvent>::BroadCast(winHandle, &IWindowEvent::OnMouseButtonUp, event.button);
            break;
        }
        case SDL_MOUSEMOTION: {
            const SDL_MouseMotionEvent &event = sdlEvent.motion;
            Event<IWindowEvent>::BroadCast(winHandle, &IWindowEvent::OnMouseMove, event.x, event.y);
            break;
        }
        case SDL_MOUSEWHEEL: {
            const SDL_MouseWheelEvent &event = sdlEvent.wheel;
            Event<IWindowEvent>::BroadCast(winHandle, &IWindowEvent::OnMouseWheel, event.x, event.y);
            break;
        }
        case SDL_KEYDOWN: {
            const SDL_KeyboardEvent &event = sdlEvent.key;
            Event<IWindowEvent>::BroadCast(winHandle, &IWindowEvent::OnKeyDown, event.keysym.scancode - 3);
            break;
        }
        case SDL_KEYUP: {
            const SDL_KeyboardEvent &event = sdlEvent.key;
            Event<IWindowEvent>::BroadCast(winHandle, &IWindowEvent::OnKeyUp, event.keysym.scancode - 3);
            break;
        }
        case SDL_TEXTINPUT: {
            const SDL_TextInputEvent &event = sdlEvent.text;
            Event<IWindowEvent>::BroadCast(winHandle, &IWindowEvent::OnTextInput, event.text);
        }
        default: break;
        }
    }
}