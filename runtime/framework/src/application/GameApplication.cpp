//
// Created by Zach Lee on 2022/9/25.
//

#include <framework/application/GameApplication.h>

#include <cxxopts.hpp>

#include <core/logger/Logger.h>
#include <core/file/FileIO.h>
#include <rapidjson/rapidjson.h>
#include <rapidjson/document.h>

#include <filesystem>

#include <framework/asset/AssetManager.h>
#include <framework/platform/PlatformBase.h>

static const char *TAG = "Application";
#ifdef SKY_EDITOR
static const char *CONFIG_PATH = "config/modules_game.json";
#else
static const char *CONFIG_PATH = "config/modules_game.json";
#endif

namespace sky {

    bool GameApplication::Init(int argc, char **argv)
    {
#ifdef SKY_EDITOR
        cxxopts::Options options("GameApplication Launcher", "SkyEngine Launcher");
        options.allow_unrecognised_options();

        options.add_options()("p,project", "Project Directory", cxxopts::value<std::string>());
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
        workFs = Platform::Get()->GetBundleFileSystem();
//        workFs = std::make_shared<NativeFileSystem>(Platform::Get()->GetInternalPath());
//        AssetManager::Get()->SetWorkPath(workFs);
#else
        AssetManager::Get()->SetWorkPath(Platform::Get()->GetInternalPath());
        auto fs = std::make_shared<NativeFileSystem>();
        fs->AddPath(Platform::Get()->GetInternalPath());
        workFs = fs;
#endif

        if (!Application::Init(argc, argv)) {
            return false;
        }

        return true;
    }

    void GameApplication::PreTick()
    {
    }

    void GameApplication::LoadConfigs()
    {
        std::string json;
        auto file = workFs->OpenFile(CONFIG_PATH);
        if (!file || file->ReadString(json)) {
            LOG_W(TAG, "Load Config Failed");
            return;
        }

        rapidjson::Document document;
        document.Parse(json.c_str());

        if (document.HasMember("modules")) {
            auto array = document["modules"].GetArray();
            for (auto &module : array) {
                if (!module.HasMember("name")) {
                    continue;
                }

                ModuleInfo info;
                info.name = module["name"].GetString();

                if (module.HasMember("dependencies")) {
                    auto depArray = module["dependencies"].GetArray();
                    for (auto &dep : depArray) {
                        info.dependencies.emplace_back(dep.GetString());
                    }
                }
                moduleManager->RegisterModule(info);
            }
        }

        if (document.HasMember("game")) {
            auto obj = document["game"].GetObject();
            if (obj.HasMember("width")) {
                width = obj["width"].GetUint();
            }
            if (obj.HasMember("height")) {
                height = obj["height"].GetUint();
            }
        }
    }

    void GameApplication::PreInit()
    {
        auto *handle = Platform::Get()->GetMainWinHandle();
        nativeWindow.reset(NativeWindow::Create(NativeWindow::Descriptor{width, height, "SkyGame", "SkyGame", handle}));
    }

    void GameApplication::PostInit()
    {
    }
}
