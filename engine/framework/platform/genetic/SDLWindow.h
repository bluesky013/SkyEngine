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

        bool Init(const Descriptor &desc) override;
        virtual void UpdateWindow() {}

        void Dispatch(const SDL_WindowEvent &event);
    protected:
        SDL_Window *window = nullptr;
    };

}
