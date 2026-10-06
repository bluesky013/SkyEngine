//
// Created by Zach Lee on 2022/9/25.
//

#pragma once

#include <framework/window/NativeWindow.h>

namespace sky {

    // Native Win32 window. Windows API types are kept out of this header
    // (they leak macros that break engine headers); all of it lives in the .cpp.
    class Win32Window : public NativeWindow {
    public:
        Win32Window() = default;
        ~Win32Window() override;

        bool Init(const Descriptor &desc) override;
        void *GetNativeHandle() const override;
        void SetPointerCapture(bool capture) override;
        bool GetGlobalCursorPosition(int32_t &x, int32_t &y) const override;
        void SetCursor(StandardCursor cursor) override;
        float GetDpiScale() const override;

        void *GetHwnd() const { return hwnd; }

        // Applies the requested cursor to the OS (used by WM_SETCURSOR too).
        void ApplyCursor() const;

        // True when this is the process main window (the first created); closing
        // it requests application exit. Secondary windows (e.g. a preview) close
        // independently.
        bool IsMainWindow() const;

        static bool EnsureWindowClass();

    private:
        void        *hwnd = nullptr;
        StandardCursor desiredCursor = StandardCursor::Arrow;
    };

} // namespace sky
