//
// Created on 2026/09/21.
//

#pragma once

#include <cstdint>
#include <framework/application/Application.h>
#include <framework/window/NativeWindow.h>
#include <memory>

namespace sky::editor::sandbox {

    // Non-Qt editor host. Loads `SandboxModule` (which owns the Aurora frame)
    // and provides the native main window. `--frames N` exits after N frames
    // for headless/dev validation.
    class EditorApplication : public sky::Application {
    public:
        EditorApplication() = default;
        ~EditorApplication() override;

        void *GetMainWindowHandle() const override;

    protected:
        void ParseStartArgs() override;
        bool LoadConfigs() override;
        bool PreInit() override;
        void PreTick() override;

    private:
        void LoadWindowGeometry();
        void SaveWindowGeometry();

        std::unique_ptr<sky::NativeWindow> window;
        uint32_t                           width            = 1280;
        uint32_t                           height           = 720;
        uint32_t                           maxFrames        = 0;
        uint32_t                           frameCount       = 0;
        int32_t                            windowX          = 0;
        int32_t                            windowY          = 0;
        bool                               hasSavedPosition = false;
    };

} // namespace sky::editor::sandbox
