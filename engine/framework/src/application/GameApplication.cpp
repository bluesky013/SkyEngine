//
// Created by Zach Lee on 2022/9/25.
//

#include <framework/application/GameApplication.h>

#include <core/cmdline/CmdParser.h>
#include <string>

#include <core/logger/Logger.h>
#include <core/file/FileIO.h>
#include <core/file/FileSystem.h>
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>

#include <filesystem>

#include <framework/asset/AssetManager.h>
#include <framework/asset/AssetDataBase.h>
#include <framework/platform/PlatformBase.h>

static const char *TAG = "Application";
static const char *CONFIG_PATH = "configs/modules_game.json";

namespace sky {

    bool GameApplication::Init(int argc, char **argv)
    {
#if __ANDROID__
        workFs = new NativeFileSystem(Platform::Get()->GetInternalPath());//Platform::Get()->GetBundleFileSystem();
//        workFs = std::make_shared<NativeFileSystem>(Platform::Get()->GetInternalPath());
//        AssetManager::Get()->SetWorkPath(workFs);

        std::string bundleKey = "common";
        auto bundleFs = workFs->CreateSubSystem(bundleKey, false);
        AssetManager::Get()->AddAssetProductBundle(new HashedAssetBundle(bundleFs, bundleKey));

#else
        // No project argument outside editor mode: use the bundle path so the
        // builtin configs/assets deployed next to the executable are found.
        workFs = new NativeFileSystem(Platform::Get()->GetBundlePath());
        AssetManager::Get()->SetWorkFileSystem(workFs);
#endif
        if (!Application::Init(argc, argv)) {
            return false;
        }
        return true;
    }

    void GameApplication::ParseStartArgs()
    {
        // `--project <path>`: mount the project directory as the work file system
        // (so its configs/ and assets/ resolve) and remember its descriptor.
        const auto &values = arguments.values;
        for (auto it = values.begin(); it != values.end(); ++it) {
            if (*it != "--project") {
                continue;
            }
            auto next = std::next(it);
            if (next == values.end()) {
                LOG_E(TAG, "--project requires a path argument");
                break;
            }
            // Accept either the .skyproj file or the project directory.
            std::string path = *next;
            std::error_code error;
            if (std::filesystem::is_directory(path, error)) {
                for (const auto &entry : std::filesystem::directory_iterator(path, error)) {
                    if (entry.path().extension() == ".skyproj") {
                        path = entry.path().string();
                        break;
                    }
                }
            }
            if (project.Read(path)) {
                hasProject = true;
                workFs     = new NativeFileSystem(project.Dir());
                AssetManager::Get()->SetWorkFileSystem(workFs);
                LOG_I(TAG, "Project mounted: %s (%s)", project.name.c_str(), project.Dir().c_str());
            } else {
                LOG_E(TAG, "Failed to read project: %s", path.c_str());
            }
            break;
        }

        // Apply the project's preferred RHI backend unless the CLI overrides it
        // (`-r`/`--rhi`). AuroraModule parses this from the module arguments.
        if (hasProject && !project.rhi.empty()) {
            bool overridden = false;
            for (const auto &value : arguments.values) {
                if (value == "--rhi" || value == "-r") {
                    overridden = true;
                    break;
                }
            }
            if (!overridden) {
                arguments.values.emplace_back("--rhi");
                arguments.args.emplace_back(arguments.values.back().c_str());
                arguments.values.emplace_back(project.rhi);
                arguments.args.emplace_back(arguments.values.back().c_str());
            }
        }
    }

    void GameApplication::PreTick()
    {
    }

    bool GameApplication::LoadConfigs()
    {
        std::unordered_map<std::string, ModuleInfo> modules = {};
        modules.emplace("Aurora.Render", ModuleInfo{"Aurora.Render", {}});
        for (auto &[key, info] : modules) {
            moduleManager->RegisterModule(info);
        }

        // Project modules come from the descriptor, so a project still loads its
        // module even without a configs/modules_game.json.
        if (hasProject) {
            for (const auto &id : project.modules) {
                moduleManager->RegisterModule(ModuleInfo{id, {}});
            }
        }

        std::string json;
        FilePtr file;
        if (workFs != nullptr) {
            file = workFs->OpenFile(CONFIG_PATH);
        }
        if (!file) {
            // Fall back to the engine builtin configs shipped next to the
            // executable (configs/ -> <exe>/configs).
            FileSystemPtr builtinFs = new NativeFileSystem(Platform::Get()->GetBundlePath());
            if (builtinFs != nullptr) {
                file = builtinFs->OpenFile(CONFIG_PATH);
                if (file) {
                    workFs = builtinFs;
                }
            }
        }
        if (!file || !file->ReadString(json)) {
            LOG_E(TAG, "Load Config Failed: %s", CONFIG_PATH);
            return false;
        }

        rapidjson::Document document;
        document.Parse(json.c_str());
        if (document.HasParseError()) {
            LOG_E(TAG, "Parse Config Failed: %s (%u)", rapidjson::GetParseError_En(document.GetParseError()), static_cast<uint32_t>(document.GetErrorOffset()));
            return false;
        }

        if (!document.IsObject()) {
            LOG_E(TAG, "Config Root Is Not An Object: %s", CONFIG_PATH);
            return false;
        }

        if (document.HasMember("modules")) {
            if (!document["modules"].IsArray()) {
                LOG_E(TAG, "Config 'modules' Is Not An Array: %s", CONFIG_PATH);
                return false;
            }
            auto array = document["modules"].GetArray();
            for (auto &module : array) {
                if (!module.IsObject()) {
                    LOG_E(TAG, "Invalid Module Entry In Config: %s", CONFIG_PATH);
                    return false;
                }

                if (!module.HasMember("name") || !module["name"].IsString()) {
                    continue;
                }

                ModuleInfo info;
                info.name = module["name"].GetString();

                if (module.HasMember("dependencies")) {
                    if (!module["dependencies"].IsArray()) {
                        LOG_E(TAG, "Module Dependencies Is Not An Array: %s", info.name.c_str());
                        return false;
                    }
                    auto depArray = module["dependencies"].GetArray();
                    for (auto &dep : depArray) {
                        if (!dep.IsString()) {
                            LOG_E(TAG, "Module Dependency Is Not A String: %s", info.name.c_str());
                            return false;
                        }
                        info.dependencies.emplace_back(dep.GetString());
                    }
                }
                moduleManager->RegisterModule(info);
            }
        }

        if (document.HasMember("game")) {
            if (!document["game"].IsObject()) {
                LOG_E(TAG, "Config 'game' Is Not An Object: %s", CONFIG_PATH);
                return false;
            }
            auto obj = document["game"].GetObject();
            if (obj.HasMember("width")) {
                if (!obj["width"].IsUint() || obj["width"].GetUint() == 0) {
                    LOG_E(TAG, "Config 'game.width' Is Invalid: %s", CONFIG_PATH);
                    return false;
                }
                width = obj["width"].GetUint();
            }
            if (obj.HasMember("height")) {
                if (!obj["height"].IsUint() || obj["height"].GetUint() == 0) {
                    LOG_E(TAG, "Config 'game.height' Is Invalid: %s", CONFIG_PATH);
                    return false;
                }
                height = obj["height"].GetUint();
            }
        }

        return true;
    }

    bool GameApplication::PreInit()
    {
        auto *handle = Platform::Get()->GetMainWinHandle();
        nativeWindow.reset(NativeWindow::Create(NativeWindow::Descriptor{width, height, "SkyGame", "SkyGame", handle}));
        if (nativeWindow == nullptr) {
            LOG_E(TAG, "Create Native Window Failed");
            return false;
        }
        return true;
    }

    void GameApplication::PostInit()
    {
    }

    void *GameApplication::GetMainWindowHandle() const
    {
        return nativeWindow != nullptr ? nativeWindow->GetNativeHandle() : nullptr;
    }
}
