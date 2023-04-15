//
// Created by Zach Lee on 2022/9/25.
//

#pragma once

#include <framework/window/NativeWindow.h>
#include <SDL2/SDL.h>

namespace sky {

    class SDLWindow : public NativeWindow {
    public:
        SDLWindow() = default;
        ~SDLWindow() override;

        void PollEvent(bool &quit) override;

        bool Init(const Descriptor &desc) override;

    protected:
        void Dispatch(const SDL_WindowEvent &event);
        void Dispatch(const SDL_Event &sdlEvent, bool &quit);

        SDL_Window *window = nullptr;
    };

}
