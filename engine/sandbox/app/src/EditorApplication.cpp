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

#include <string>
#include <vector>

static const char *TAG = "EditorApplication";
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

    bool EditorApplication::LoadConfigs()
    {
        // Load the editor module list from the builtin config deployed next to
        // the executable; fall back to SandboxModule when the config is absent
        // (e.g. the editor is launched without the launcher's configs/).
        const std::string configPath = Platform::Get()->GetBundlePath() + "/" + EDITOR_CONFIG_PATH;

        std::string json;
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
