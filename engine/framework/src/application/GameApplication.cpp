//
// Created by Zach Lee on 2022/9/25.
//

#include <framework/application/GameApplication.h>

#include <core/cmdline/CmdParser.h>
#include <string>

#include <core/logger/Logger.h>
#include <core/file/FileIO.h>
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>

#include <framework/asset/AssetManager.h>
#include <framework/asset/AssetDataBase.h>
#include <framework/platform/PlatformBase.h>

static const char *TAG = "Application";
#ifdef SKY_EDITOR
static const char *CONFIG_PATH = "config/modules_game.json";
#else
static const char *CONFIG_PATH = "configs/modules_game.json";
#endif

namespace sky {

    bool GameApplication::Init(int argc, char **argv)
    {
#ifdef SKY_EDITOR
        CmdOptions options("GameApplication Launcher", "SkyEngine Launcher");
        options.allow_unrecognised_options();

        options.add_options()("p,project", "Project Directory", CmdValue<std::string>());
        auto result = options.parse(argc, argv);
        if (result.count("project") != 0u) {
            std::string projectPath = result["project"].as<std::string>();
            workFs = new NativeFileSystem(projectPath);
            AssetManager::Get()->SetWorkFileSystem(workFs);
        } else {
            std::string workPath = Platform::Get()->GetBundlePath();
            workFs = new NativeFileSystem(workPath);
            AssetManager::Get()->SetWorkFileSystem(workFs);
        }
#elif __ANDROID__
        workFs = new NativeFileSystem(Platform::Get()->GetInternalPath());//Platform::Get()->GetBundleFileSystem();
//        workFs = std::make_shared<NativeFileSystem>(Platform::Get()->GetInternalPath());
//        AssetManager::Get()->SetWorkPath(workFs);
        AssetDataBase::Get()->SetWorkSpaceFs(workFs);
        AssetDataBase::Get()->Load();

        std::string bundleKey = "common";
        auto bundleFs = workFs->CreateSubSystem(bundleKey, false);
        AssetManager::Get()->AddAssetProductBundle(new HashedAssetBundle(bundleFs, bundleKey));

#else
        // AssetManager::Get()->SetWorkPath(Platform::Get()->GetInternalPath());
        // auto fs = std::make_shared<NativeFileSystem>();
        // fs->AddPath(Platform::Get()->GetInternalPath());
        // workFs = fs;
#endif
        if (!Application::Init(argc, argv)) {
            return false;
        }
        return true;
    }

    void GameApplication::PreTick()
    {
    }

    bool GameApplication::LoadConfigs()
    {
        std::unordered_map<std::string, ModuleInfo> modules = {};
        modules.emplace("SkyRender", ModuleInfo{"SkyRender", {"ShaderCompiler"}});
        for (auto &[key, info] : modules) {
            moduleManager->RegisterModule(info);
        }

        std::string json;
        auto file = workFs->OpenFile(CONFIG_PATH);
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
}
