//
// Created by Zach Lee on 2021/11/10.
//

#pragma once

#include <string>
#include <framework/window/IWindow.h>
#include <framework/window/IWindowEvent.h>

//struct SDL_Window;
//union SDL_Event;
//struct SDL_WindowEvent;

namespace sky {
    class IWindowEvent;

    class NativeWindow : public IWindow {
    public:
        NativeWindow();
        ~NativeWindow() override;

        struct Descriptor {
            uint32_t width = 1366;
            uint32_t height = 768;
            std::string className;
            std::string titleName;
            void *handle = nullptr;
        };

        static NativeWindow *Create(const Descriptor &);

        void *GetNativeHandle() const override;

        virtual bool Init(const Descriptor &desc) { return true; }

        uint32_t GetWidth() const { return descriptor.width; }
        uint32_t GetHeight() const { return descriptor.height; }

        WindowID GetWinId() const { return winID; }
    protected:
        friend class NativeWindowManager;

        void SetID(WindowID id);

        void *winHandle = nullptr;
        WindowID winID = INVALID_WIN_ID;
        Descriptor descriptor;
    };
} // namespace sky
