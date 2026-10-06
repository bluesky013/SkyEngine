//
// Native Cocoa window (no SDL).
//

#pragma once

#include <framework/window/NativeWindow.h>

namespace sky {

    // Ensures NSApplication exists, is activated, and has a minimal app menu
    // (Cmd+Q). Safe to call repeatedly; implemented in CocoaWindow.mm.
    bool EnsureNSApplication();

    // Native Cocoa window (no SDL). AppKit types are kept out of this header;
    // all of it lives in the .mm. The content view is backed by a CAMetalLayer;
    // GetNativeHandle() returns that layer, which is what both the Metal
    // swapchain and the MoltenVK surface path consume.
    class CocoaWindow : public NativeWindow {
    public:
        CocoaWindow() = default;
        ~CocoaWindow() override;

        bool Init(const Descriptor &desc) override;
        void *GetNativeHandle() const override;

        // Device pixels / point; queried live from the window so it tracks moves
        // between displays with different backing scale factors.
        float GetDpiScale() const override;

        void *GetNSWindow() const { return window; }

    private:
        void *window     = nullptr; // NSWindow, owned
        void *delegate   = nullptr; // SkyCocoaWindowDelegate, owned
        void *metalLayer = nullptr; // CAMetalLayer of the content view, not owned
    };

} // namespace sky
