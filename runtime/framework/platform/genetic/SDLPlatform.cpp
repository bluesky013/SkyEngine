//
// Created by Zach Lee on 2022/9/25.
//

#include "SDLPlatform.h"
#include <core/logger/Logger.h>
#include <SDL2/SDL.h>
#include <framework/window/IWindowEvent.h>
#include <framework/window/NativeWindowManager.h>

#include "SDLWindow.h"

static const char* TAG = "SDLPlatform";

namespace sky {

    SDLPlatform::~SDLPlatform()
    {
        SDL_Quit();
    }

    bool SDLPlatform::Init(const PlatformInfo &desc)
    {
        if (SDL_Init(SDL_INIT_EVERYTHING) < 0) {
            LOG_E(TAG, "SDL could not be initialized! Error: %s", SDL_GetError());
            return false;
        }

        SDL_EventState(SDL_DROPFILE, SDL_ENABLE);
        return true;
    }

    uint64_t SDLPlatform::GetPerformanceFrequency() const
    {
        return SDL_GetPerformanceFrequency();
    }

    uint64_t SDLPlatform::GetPerformanceCounter() const
    {
        return SDL_GetPerformanceCounter();
    }

    char* SDLPlatform::GetClipBoardText()
    {
        return SDL_GetClipboardText();
    }

    void SDLPlatform::FreeClipBoardText(char* text)
    {
        SDL_free(text);
    }

    void SDLPlatform::SetClipBoardText(const std::string &text)
    {
        SDL_SetClipboardText(text.data());
    }

    void SDLPlatform::PollEvent(bool &exit)
    {
        SDL_Event sdlEvent;
        while (SDL_PollEvent(&sdlEvent) != 0) {
            Dispatch(sdlEvent, exit);
        }
    }

    void SDLPlatform::DispatchWindowEvent(const SDL_Event &sdlEvent)
    {
        const auto& winEvent = sdlEvent.window;
        auto *nativeWindow = NativeWindowManager::Get()->GetWindowByID(winEvent.windowID);
        if (nativeWindow != nullptr) {
            static_cast<SDLWindow*>(nativeWindow)->Dispatch(winEvent);
        }
    }

    void SDLPlatform::Dispatch(const SDL_Event &sdlEvent, bool &quit)
    {
        switch (sdlEvent.type) {
            case SDL_QUIT: {
                quit = true;
                break;
            }
            case SDL_WINDOWEVENT:
                DispatchWindowEvent(sdlEvent);
                break;
            case SDL_DROPFILE:
            {
                auto *droppedFile = sdlEvent.drop.file;
                Event<IDropEvent>::BroadCast(&IDropEvent::OnDrop, std::string(droppedFile));
                SDL_free(droppedFile);
                break;
            }
            case SDL_MOUSEBUTTONUP:
            case SDL_MOUSEBUTTONDOWN: {
                const SDL_MouseButtonEvent &event = sdlEvent.button;
                MouseButtonEvent mEvent = {};
                mEvent.winID = event.windowID;
                mEvent.x = event.x;
                mEvent.y = event.y;
                mEvent.clicks = event.clicks;
                if (event.button == SDL_BUTTON_LEFT) { mEvent.button = MouseButtonType::LEFT; }
                else if (event.button == SDL_BUTTON_RIGHT) { mEvent.button = MouseButtonType::RIGHT; }
                else if (event.button == SDL_BUTTON_MIDDLE) { mEvent.button = MouseButtonType::MIDDLE; }
                if (sdlEvent.type == SDL_MOUSEBUTTONUP) {
                    Event<IMouseEvent>::BroadCast(&IMouseEvent::OnMouseButtonUp, mEvent);
                } else {
                    Event<IMouseEvent>::BroadCast(&IMouseEvent::OnMouseButtonDown, mEvent);
                }
                break;
            }
            case SDL_MOUSEMOTION: {
                const SDL_MouseMotionEvent &event = sdlEvent.motion;
                MouseMotionEvent mEvent = {};
                mEvent.winID = event.windowID;
                mEvent.x = event.x;
                mEvent.y = event.y;
                mEvent.relX = event.xrel;
                mEvent.relY = event.yrel;
                Event<IMouseEvent>::BroadCast(&IMouseEvent::OnMouseMotion, mEvent);
                break;
            }
            case SDL_MOUSEWHEEL: {
                const SDL_MouseWheelEvent &event = sdlEvent.wheel;
                MouseWheelEvent mEvent = {};
                mEvent.winID = event.windowID;
                mEvent.x = event.x;
                mEvent.y = event.y;
                Event<IMouseEvent>::BroadCast(&IMouseEvent::OnMouseWheel, mEvent);
                break;
            }
            case SDL_KEYUP:
            case SDL_KEYDOWN: {
                const SDL_KeyboardEvent &event = sdlEvent.key;
                KeyboardEvent kEvent = {};
                kEvent.winID = event.windowID;
                kEvent.scanCode = static_cast<ScanCode>(event.keysym.scancode);
                kEvent.mod = KeyModFlags(event.keysym.mod);
                if (sdlEvent.type == SDL_KEYUP) {
                    Event<IKeyboardEvent>::BroadCast(&IKeyboardEvent::OnKeyUp, kEvent);
                } else {
                    Event<IKeyboardEvent>::BroadCast(&IKeyboardEvent::OnKeyDown, kEvent);
                }
                break;
            }
            case SDL_TEXTINPUT: {
                const SDL_TextInputEvent &event = sdlEvent.text;
                Event<IKeyboardEvent>::BroadCast(&IKeyboardEvent::OnTextInput, event.windowID, event.text);
            }
            default: break;
        }
    }

//    std::string SDLPlatform::GetBundlePath() const
//    {
//        return SDL_GetBasePath();
//    }
} // namespace sky