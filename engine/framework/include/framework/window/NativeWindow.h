//
// Created by Zach Lee on 2021/11/10.
//

#pragma once

#include <framework/window/Cursor.h>
#include <framework/window/IWindow.h>
#include <framework/window/IWindowEvent.h>
#include <string>

namespace sky {
    class IWindowEvent;

    class NativeWindow : public IWindow {
    public:
        NativeWindow();
        ~NativeWindow() override;

        struct Descriptor {
            uint32_t    width  = 1366;
            uint32_t    height = 768;
            std::string className;
            std::string titleName;
            void       *handle = nullptr;
        };

        static NativeWindow *Create(const Descriptor &);

        void *GetNativeHandle() const override;

        virtual bool Init(const Descriptor &desc)
        {
            return true;
        }

        uint32_t GetWidth() const
        {
            return descriptor.width;
        }
        uint32_t GetHeight() const
        {
            return descriptor.height;
        }

        WindowID GetWinId() const
        {
            return winID;
        }

        // Window client size/position in screen pixels (live). Backends without
        // support leave these as no-ops/false.
        virtual bool GetPosition(int32_t &x, int32_t &y) const
        {
            (void)x;
            (void)y;
            return false;
        }
        virtual void SetPosition(int32_t x, int32_t y)
        {
            (void)x;
            (void)y;
        }

        // OS-level pointer capture so a drag keeps delivering motion to this
        // window even when the cursor leaves its client area. Backends without
        // support leave it as a no-op.
        virtual void SetPointerCapture(bool capture)
        {
        }

        // Cursor position in global (screen) pixels; used to resolve cross-window
        // drags. Backends without support return false.
        virtual bool GetGlobalCursorPosition(int32_t &x, int32_t &y) const
        {
            return false;
        }

        // Requests the OS cursor shown over this window (e.g. a resize cursor
        // over a splitter). Backends without support leave it as a no-op.
        virtual void SetCursor(StandardCursor cursor)
        {
        }

        // Window DPI scale (device pixels / 96). Backends default to 1.
        virtual float GetDpiScale() const
        {
            return 1.0f;
        }

    protected:
        friend class NativeWindowManager;

        void SetID(WindowID id);

        void      *winHandle = nullptr;
        WindowID   winID     = INVALID_WIN_ID;
        float      scale     = 1.f;
        Descriptor descriptor;
    };
} // namespace sky
