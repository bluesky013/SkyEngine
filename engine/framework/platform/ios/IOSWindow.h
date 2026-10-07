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
        IOSWindow()           = default;
        ~IOSWindow() override;

        bool Init(const Descriptor &desc) override;
        void *GetNativeHandle() const override;
        float GetDpiScale() const override;
        void SetTitle(const std::string &title) override;

    private:
        void *window     = nullptr; // UIWindow, owned
        void *controller = nullptr; // UIViewController, retained by the window
        void *metalLayer = nullptr; // CAMetalLayer of the content view
    };

} // namespace sky
