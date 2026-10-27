//
// Native X11 window (no SDL).
//

#pragma once

#include <core/platform/NativeWindowHandle.h>
#include <framework/window/NativeWindow.h>

namespace sky {

    // Native X11 window. The Xlib/XCB types are kept out of this header; they
    // live in the .cpp. GetNativeHandle() returns the shared XcbWindowHandle,
    // which is what the Vulkan xcb surface path consumes.
    class LinuxWindow : public NativeWindow {
    public:
        LinuxWindow() = default;
        ~LinuxWindow() override;

        bool  Init(const Descriptor &desc) override;
        void *GetNativeHandle() const override
        {
            return winHandle;
        }

        bool GetPosition(int32_t &x, int32_t &y) const override;
        void SetPosition(int32_t x, int32_t y) override;
        void SetPointerCapture(bool capture) override;
        bool GetGlobalCursorPosition(int32_t &x, int32_t &y) const override;
        void SetCursor(StandardCursor cursor) override;
        void SetTitle(const std::string &title) override;
        bool IsMainWindow() const override
        {
            return mainWindow;
        }

        // Called by LinuxPlatform::PollEvent for events targeting this window.
        void DispatchNativeEvent(void *event);

        void NotifyResized(uint32_t width, uint32_t height)
        {
            descriptor.width  = width;
            descriptor.height = height;
        }

    private:
        void ApplyCursor();
        void UpdateXcbHandle();

        void         *display    = nullptr; // Display*
        unsigned long xwindow    = 0;       // XID
        XcbWindowHandle xcbHandle;
        bool          mainWindow = false;
        StandardCursor desiredCursor = StandardCursor::Arrow;
        unsigned long  cursorHandle  = 0;
    };

} // namespace sky
