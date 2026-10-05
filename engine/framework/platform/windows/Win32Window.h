//
// Created by Zach Lee on 2022/9/25.
//

#pragma once

#include <framework/window/NativeWindow.h>

namespace sky {

    // Native Win32 window (no SDL). Windows API types are kept out of this header
    // (they leak macros that break engine headers); all of it lives in the .cpp.
    class Win32Window : public NativeWindow {
    public:
        Win32Window() = default;
        ~Win32Window() override;

        bool Init(const Descriptor &desc) override;
        void *GetNativeHandle() const override;

        void *GetHwnd() const { return hwnd; }

        // True when this is the process main window (the first created); closing
        // it requests application exit. Secondary windows (e.g. a preview) close
        // independently.
        bool IsMainWindow() const;

        static bool EnsureWindowClass();

    private:
        void *hwnd = nullptr;
    };

} // namespace sky
