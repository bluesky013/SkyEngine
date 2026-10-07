//
// Created on 2026/09/21.
//

#include "EditorApplication.h"

#include <core/cmdline/CmdParser.h>
#include <core/file/FileIO.h>
#include <core/logger/Logger.h>
#include <framework/application/ModuleManager.h>
#include <framework/platform/PlatformBase.h>

#include <rapidjson/document.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#if defined(_WIN32)
    #include <windows.h>
#endif

static const char *TAG                = "EditorApplication";
static const char *EDITOR_CONFIG_PATH = "configs/modules_editor.json";

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

    namespace {
        // Parses {"modules":[{"name":..,"dependencies":[..]}]} into module infos.
        bool ReadModules(const std::string &json, std::vector<ModuleInfo> &out)
        {
            rapidjson::Document document;
            document.Parse(json.c_str());
            if (document.HasParseError() || !document.IsObject()) {
                return false;
            }
            if (!document.HasMember("modules") || !document["modules"].IsArray()) {
                return false;
            }
            for (const auto &module : document["modules"].GetArray()) {
                if (!module.IsObject() || !module.HasMember("name") || !module["name"].IsString()) {
                    continue;
                }
                ModuleInfo info;
                info.name = module["name"].GetString();
                if (module.HasMember("dependencies") && module["dependencies"].IsArray()) {
                    for (const auto &dep : module["dependencies"].GetArray()) {
                        if (dep.IsString()) {
                            info.dependencies.emplace_back(dep.GetString());
                        }
                    }
                }
                out.push_back(std::move(info));
            }
            return true;
        }
    } // namespace

    static std::string WindowStatePath()
    {
        if (Platform::Get() == nullptr) {
            return {};
        }
        const std::string base = Platform::Get()->GetUserConfigPath();
        return base.empty() ? std::string() : base + "/skyengine/editor_window.json";
    }

    EditorApplication::~EditorApplication()
    {
        SaveWindowGeometry();
    }

    void EditorApplication::LoadWindowGeometry()
    {
        const std::string path = WindowStatePath();
        if (path.empty()) {
            return;
        }
        std::string json;
        if (!ReadString(path, json) || json.empty()) {
            return;
        }
        rapidjson::Document document;
        document.Parse(json.c_str());
        if (document.HasParseError() || !document.IsObject()) {
            return;
        }
        if (document.HasMember("width") && document["width"].IsUint() && document["width"].GetUint() >= 320u) {
            width = document["width"].GetUint();
        }
        if (document.HasMember("height") && document["height"].IsUint() && document["height"].GetUint() >= 240u) {
            height = document["height"].GetUint();
        }
        if (document.HasMember("x") && document["x"].IsInt() && document.HasMember("y") && document["y"].IsInt()) {
            windowX          = document["x"].GetInt();
            windowY          = document["y"].GetInt();
            hasSavedPosition = true;
        }
        LOG_I(TAG, "restored window geometry %ux%u from %s", width, height, path.c_str());
    }

    void EditorApplication::SaveWindowGeometry()
    {
        // Skip bounded/dev runs (-frames) so they do not clobber the saved size.
        if (window == nullptr || maxFrames > 0) {
            return;
        }
        const std::string path = WindowStatePath();
        if (path.empty()) {
            return;
        }

        const uint32_t w = window->GetWidth();
        const uint32_t h = window->GetHeight();
        int32_t        x = 0;
        int32_t        y = 0;
        window->GetPosition(x, y);

        std::error_code ec;
        std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);

        char buffer[160];
        std::snprintf(buffer, sizeof(buffer), "{\n  \"width\": %u,\n  \"height\": %u,\n  \"x\": %d,\n  \"y\": %d\n}\n", w, h, x, y);
        WriteString(path, buffer);
    }

    bool EditorApplication::LoadConfigs()
    {
        // Load the editor module list from the builtin config deployed next to
        // the executable; fall back to SandboxModule when the config is absent
        // (e.g. the editor is launched without the launcher's configs/).
        const std::string configPath = Platform::Get()->GetBundlePath() + "/" + EDITOR_CONFIG_PATH;

        std::string             json;
        std::vector<ModuleInfo> modules;
        if (ReadString(configPath, json) && ReadModules(json, modules) && !modules.empty()) {
            for (auto &info : modules) {
                moduleManager->RegisterModule(info);
            }
            LOG_I(TAG, "editor modules loaded from %s", EDITOR_CONFIG_PATH);
            return true;
        }

        LOG_W(TAG, "editor config '%s' not found; falling back to SandboxModule", EDITOR_CONFIG_PATH);
        moduleManager->RegisterModule(ModuleInfo{"SandboxModule", {}});
        return true;
    }

    bool EditorApplication::PreInit()
    {
        LoadWindowGeometry();
        window.reset(
            NativeWindow::Create(NativeWindow::Descriptor{width, height, "SandboxEditor", "SkyEngine Editor", Platform::Get()->GetMainWinHandle()}));
        if (window == nullptr) {
            LOG_E(TAG, "create native window failed");
            return false;
        }
        if (hasSavedPosition) {
            // Keep the main window reachable: clamp the restored position to the
            // primary screen so a stale position cannot hide the window off-screen.
#if defined(_WIN32)
            const int screenW = ::GetSystemMetrics(SM_CXSCREEN);
            const int screenH = ::GetSystemMetrics(SM_CYSCREEN);
            windowX           = std::max(0, std::min(windowX, std::max(0, screenW - 100)));
            windowY           = std::max(0, std::min(windowY, std::max(0, screenH - 100)));
#endif
            window->SetPosition(windowX, windowY);
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
