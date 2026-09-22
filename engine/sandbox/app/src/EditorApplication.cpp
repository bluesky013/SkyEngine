//
// Created on 2026/09/21.
//

#include "EditorApplication.h"

#include <core/cmdline/CmdParser.h>
#include <core/logger/Logger.h>
#include <framework/application/ModuleManager.h>
#include <framework/platform/PlatformBase.h>

static const char *TAG = "EditorApplication";

namespace sky::editor::sandbox {

    void EditorApplication::ParseStartArgs()
    {
        if (arguments.args.empty()) {
            return;
        }
        CmdOptions options("SandboxEditor", "SkyEngine Editor");
        options.allow_unrecognised_options();
        options.add_options()("frames", "exit after N frames", CmdValue<std::string>());

        auto result = options.parse(static_cast<int>(arguments.args.size()), arguments.args.data());
        if (result.count("frames") != 0u) {
            maxFrames = static_cast<uint32_t>(std::stoul(result["frames"].as<std::string>()));
        }
    }

    bool EditorApplication::LoadConfigs()
    {
        // Hardcoded for now: the editor host loads SandboxModule only (no
        // AuroraRender, so there is a single frame context).
        moduleManager->RegisterModule(ModuleInfo{"SandboxModule", {}});
        return true;
    }

    bool EditorApplication::PreInit()
    {
        window.reset(NativeWindow::Create(
            NativeWindow::Descriptor{width, height, "SandboxEditor", "SandboxEditor",
                                     Platform::Get()->GetMainWinHandle()}));
        if (window == nullptr) {
            LOG_E(TAG, "create native window failed");
            return false;
        }
        return true;
    }

    void EditorApplication::PreTick()
    {
        if (maxFrames > 0 && ++frameCount >= maxFrames) {
            SetExit();
        }
    }

    void *EditorApplication::GetMainWindowHandle() const
    {
        return window != nullptr ? window->GetNativeHandle() : nullptr;
    }

} // namespace sky::editor::sandbox
