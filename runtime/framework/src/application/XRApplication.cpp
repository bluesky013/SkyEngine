//
// Created by blues on 2024/3/13.
//

#include <framework/application/XRApplication.h>
#include <cxxopts.hpp>

#include <core/logger/Logger.h>
#include <core/file/FileIO.h>
#include <rapidjson/rapidjson.h>
#include <rapidjson/document.h>

#include <filesystem>

#include <framework/asset/AssetManager.h>
#include <framework/platform/PlatformBase.h>

static const char *TAG = "Application";
static const char *CONFIG_PATH = "/config/modules_game.json";

namespace sky {

    bool XRApplication::Init(int argc, char **argv)
    {
#ifdef SKY_EDITOR
        cxxopts::Options options("GameApplication Launcher", "SkyEngine Launcher");
        options.allow_unrecognised_options();

        options.add_options()("p,project", "Project Directory", cxxopts::value<std::string>());
        auto result = options.parse(argc, argv);
        if (result.count("project") != 0u) {
            AssetManager::Get()->SetProjectPath(result["project"].as<std::string>());
        } else {
            AssetManager::Get()->SetProjectPath(Platform::Get()->GetBundlePath());
        }
#else
        AssetManager::Get()->SetWorkPath(Platform::Get()->GetBundleFileSystem());
#endif

        if (!Application::Init(argc, argv)) {
            return false;
        }

        return true;
    }

    void XRApplication::Shutdown()
    {
        Application::Shutdown();
    }

    void XRApplication::LoadConfigs()
    {
#ifdef SKY_EDITOR
        auto configPath = AssetManager::Get()->GetProjectPath() + CONFIG_PATH;
#else
        auto configPath = Platform::Get()->GetInternalPath() + CONFIG_PATH;
#endif

        std::string json;
        if (!ReadString(configPath, json)) {
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
    }


} // namespace sky