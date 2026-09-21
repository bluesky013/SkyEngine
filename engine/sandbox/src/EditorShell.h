//
// Created on 2026/09/21.
//

#pragma once

#include <cstdint>
#include <editor/core/input/InputRouter.h>
#include <editor/core/layout/LayoutModel.h>
#include <editor/core/layout/PanelRegistry.h>
#include <string>

namespace sky {
    class NativeWindow;
} // namespace sky

namespace sky::editor::sandbox {

    // Minimal non-Qt editor shell for the redesign Phase 1.
    //
    // Owns the native window and drives the frame loop. It contains no UI
    // toolkit and no render dependency; shell services (clipboard, file
    // dialogs, text input/IME) are provided by the platform layer. It also
    // owns the headless editor state (layout, panel registry, input routing).
    class EditorShell {
    public:
        EditorShell() = default;
        ~EditorShell() = default;

        EditorShell(const EditorShell &) = delete;
        EditorShell &operator=(const EditorShell &) = delete;

        bool Init();
        // Runs the frame loop. maxSeconds <= 0 runs until the window closes.
        void Run(float maxSeconds = 0.f);
        void Shutdown();

        void *GetWindowHandle() const;
        bool OpenFile(std::string &outPath, const std::string &filter = {}) const;

        LayoutModel &GetLayout() { return layout; }
        PanelRegistry &GetPanelRegistry() { return panels; }
        InputRouter &GetInputRouter() { return inputRouter; }
        const std::string &GetLayoutPath() const { return layoutPath; }

    private:
        void Tick(float deltaTime);

        NativeWindow *window = nullptr;
        uint64_t frequency = 0;
        uint64_t lastCounter = 0;

        LayoutModel layout;
        PanelRegistry panels;
        InputRouter inputRouter;
        std::string layoutPath;
    };

} // namespace sky::editor::sandbox
