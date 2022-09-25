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
        virtual ~NativeWindow();

        struct Descriptor {
            uint32_t    width  = 1366;
            uint32_t    height = 768;
            std::string className;
            std::string titleName;
        };

        static NativeWindow *Create(const Descriptor &);

        void *GetNativeHandle() const;

        virtual void PollEvent(bool &quit) = 0;

        virtual bool Init(const Descriptor &desc) = 0;

    protected:
        void *winHandle = nullptr;
    };
} // namespace sky