//
// Native UIKit window for iOS.
//

#pragma once

#include <framework/window/NativeWindow.h>

namespace sky {

    // UIKit window: a UIViewController whose view is backed by a CAMetalLayer.
    // GetNativeHandle() returns that layer (the RHI surface handle on iOS).
    class IOSWindow : public NativeWindow {
    public:
        IOSWindow() = default;
        ~IOSWindow() override;

        bool  Init(const Descriptor &desc) override;
        void *GetNativeHandle() const override;
        float GetDpiScale() const override;
        void  SetTitle(const std::string &title) override;
        bool  IsMainWindow() const override;
        void *GetWindowScene() const;

        // A disconnected scene drops only the UIKit surface. The registered
        // engine window and its ID survive until Close() or destruction.
        bool AttachScene(void *windowScene);
        void DetachScene();

        // UIKit callbacks and the platform lifecycle share these entry points.
        void SetFocused(bool focused);
        void SetTextInputEnabled(bool enabled);
        void SetTextInputRect(int32_t x, int32_t y, int32_t width, int32_t height);
        void NotifyFramebufferSize(uint32_t width, uint32_t height, float nativeScale);
        void Close();

    private:
        void RemoveFocusObservers();
        void ReleaseSurface();

        void *window           = nullptr; // UIWindow, owned
        void *focusObservers   = nullptr; // NSArray of notification tokens
        bool  focused          = false;
        bool  closed           = false;
        bool  textInputEnabled = false;
    };

} // namespace sky
