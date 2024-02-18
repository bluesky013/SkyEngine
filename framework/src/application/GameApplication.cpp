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

static const char *TAG = "Application";
static const char *CONFIG_PATH = "/config/modules_game.json";

namespace sky {

    bool GameApplication::Init(int argc, char **argv)
    {
#ifdef SKY_EDITOR
        cxxopts::Options options("GameApplication Launcher", "SkyEngine Launcher");
        options.allow_unrecognised_options();

        options.add_options()("p,project", "Project Directory", cxxopts::value<std::string>());
        auto result = options.parse(argc, argv);
        if (result.count("project") != 0u) {
            AssetManager::Get()->SetProjectPath(result["project"].as<std::string>());
        }
#endif

        if (!Application::Init(argc, argv)) {
            return false;
        }

        return true;
    }

    void GameApplication::Shutdown()
    {
        Application::Shutdown();
        nativeWindow.reset();
    }

    void GameApplication::PreTick()
    {
        if (nativeWindow) {
            nativeWindow->PollEvent(exit);
        }
    }

    const NativeWindow *GameApplication::GetViewport() const
    {
        return nativeWindow.get();
    }

    void GameApplication::LoadConfigs()
    {
#ifdef SKY_EDITOR
        auto projectPath = AssetManager::Get()->GetProjectPath();

        std::string json;
        if (!ReadString(projectPath + CONFIG_PATH, json)) {
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
#endif
    }

    void GameApplication::PreInit()
    {
        nativeWindow.reset(NativeWindow::Create(NativeWindow::Descriptor{width, height, "SkyGame", "SkyGame", nullptr}));
    }

    void GameApplication::PostInit()
    {
    }
}