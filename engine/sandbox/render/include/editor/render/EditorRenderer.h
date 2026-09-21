//
// Created on 2026/09/21.
//

#pragma once

#include <cstdint>

namespace sky::editor {

    // Editor-owned renderer.
    //
    // Records its own passes on the Aurora device and presents to the editor
    // swapchain, implementing the GUI pipeline for `sky::ui` draw data. It is
    // independent of the engine scene pipeline; the 3D viewport is composited
    // later (offscreen scene target or a shared texture from a PIE process).
    //
    // This module links Aurora + UI, so it is separate from the render- and
    // toolkit-independent EditorCore.
    class EditorRenderer {
    public:
        EditorRenderer() = default;
        ~EditorRenderer() = default;

        EditorRenderer(const EditorRenderer &) = delete;
        EditorRenderer &operator=(const EditorRenderer &) = delete;

        // windowHandle is the native handle (HWND / CAMetalLayer) of the editor
        // main window.
        bool Init(void *windowHandle, uint32_t width, uint32_t height);
        void Shutdown();
        void RenderFrame(float deltaTime);

        bool IsInitialized() const { return initialized; }
        uint32_t GetWidth() const { return width; }
        uint32_t GetHeight() const { return height; }

    private:
        bool initialized = false;
        uint32_t width = 0;
        uint32_t height = 0;
    };

} // namespace sky::editor
