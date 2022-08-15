//
// Created by Zach Lee on 2021/11/10.
//


#pragma once

#include <string>

struct SDL_Window;
union SDL_Event;
struct SDL_WindowEvent;

namespace sky {
    class IWindowEvent;

    class NativeWindow {
    public:
        NativeWindow();
        ~NativeWindow();

        struct Descriptor {
            uint32_t width = 1366;
            uint32_t height = 768;
            std::string className;
            std::string titleName;
        };

        static NativeWindow* Create(const Descriptor&);

        void* GetNativeHandle() const;

        void PollEvent(bool &quit);
    private:
        bool Init(const Descriptor&);

        void Dispatch(const SDL_Event &event, bool &quit);

        void Dispatch(const SDL_WindowEvent &event);

        SDL_Window *window = nullptr;
        void* winHandle = nullptr;
    };
}