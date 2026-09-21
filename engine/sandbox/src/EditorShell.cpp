//
// Created on 2026/09/21.
//

#include "EditorShell.h"
#include <core/logger/Logger.h>
#include <editor/core/EditorCore.h>
#include <editor/core/layout/DefaultPanels.h>
#include <editor/core/layout/LayoutPersistence.h>
#include <framework/platform/PlatformBase.h>
#include <framework/window/NativeWindow.h>
#include <string>
#include <vector>

static const char *TAG = "EditorShell";

using namespace sky;

namespace sky::editor::sandbox {

    bool EditorShell::Init()
    {
        Platform *platform = Platform::Get();
        if (!platform->Init({})) {
            LOG_E(TAG, "Platform init failed");
            return false;
        }

        EditorCore::Init();

        // Default panel registry; views arrive with the UI pass.
        RegisterDefaultEditorPanels(panels);

        layoutPath = LayoutPersistence::GetDefaultPath();
        std::vector<std::string> warnings;
        if (!layoutPath.empty() && LayoutPersistence::Load(layoutPath, layout, &panels, &warnings)) {
            LOG_I(TAG, "layout restored: %s", layoutPath.c_str());
        } else {
            layout.SetDefault({"viewport", "outliner", "inspector"});
            LOG_I(TAG, "using default layout (config: %s)",
                  layoutPath.empty() ? "<none>" : layoutPath.c_str());
        }
        for (const auto &warning : warnings) {
            LOG_W(TAG, "layout: %s", warning.c_str());
        }

        window = NativeWindow::Create(NativeWindow::Descriptor{1280, 720, "SkyEditor", "SkyEditor", nullptr});
        if (window == nullptr) {
            LOG_E(TAG, "Native window creation failed");
            return false;
        }

        frequency = platform->GetPerformanceFrequency();
        lastCounter = platform->GetPerformanceCounter();

        // Viewport focused but not consuming input, so the UI does not gate it.
        inputRouter.SetFocus("viewport");

        // Enable keyboard/IME text input for the shell.
        platform->StartTextInput();

        LOG_I(TAG, "Editor shell initialized: %ux%u", window->GetWidth(), window->GetHeight());
        return true;
    }

    void EditorShell::Tick(float deltaTime)
    {
        (void)deltaTime; // Phase 1: no rendering yet.
    }

    void EditorShell::Run(float maxSeconds)
    {
        Platform *platform = Platform::Get();
        const uint64_t startCounter = platform->GetPerformanceCounter();

        bool exit = false;
        while (!exit) {
            platform->PoolEvent(exit);

            const uint64_t now = platform->GetPerformanceCounter();
            const float deltaTime = frequency > 0
                ? static_cast<float>(static_cast<double>(now - lastCounter) / static_cast<double>(frequency))
                : 0.f;
            lastCounter = now;

            Tick(deltaTime);

            if (maxSeconds > 0.f) {
                const double elapsed =
                    static_cast<double>(now - startCounter) / static_cast<double>(frequency);
                if (elapsed > maxSeconds) {
                    break;
                }
            }
        }
    }

    void EditorShell::Shutdown()
    {
        if (!layoutPath.empty()) {
            if (LayoutPersistence::Save(layout, layoutPath)) {
                LOG_I(TAG, "layout saved: %s", layoutPath.c_str());
            } else {
                LOG_W(TAG, "layout save failed: %s", layoutPath.c_str());
            }
        }

        Platform::Get()->StopTextInput();

        delete window;
        window = nullptr;

        EditorCore::Shutdown();
    }

    void *EditorShell::GetWindowHandle() const
    {
        return window != nullptr ? window->GetNativeHandle() : nullptr;
    }

    bool EditorShell::OpenFile(std::string &outPath, const std::string &filter) const
    {
        return Platform::Get()->ShowOpenFileDialog(GetWindowHandle(), outPath, "Open File", filter);
    }

} // namespace sky::editor::sandbox
