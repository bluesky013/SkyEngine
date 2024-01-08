//
// Created by blues on 2024/1/1.
//

#include <framework/application/ToolApplicationBase.h>

#include <cxxopts.hpp>

#include <core/logger/Logger.h>
#include <core/file/FileIO.h>

#include <framework/asset/AssetManager.h>
static const char *TAG = "Application";
static const char *CONFIG_PATH = "/config/modules_tool.json";

namespace sky {
#ifdef SKY_EDITOR
    void ToolApplicationBase::ParseStartArgs()
    {
        cxxopts::Options options("Application Launcher", "SkyEngine Launcher");
        options.allow_unrecognised_options();

        options.add_options()
            ("p,project", "Project Directory", cxxopts::value<std::string>())
            ("m,module", "Addition Module", cxxopts::value<std::vector<std::string>>())
            ("h,help", "Print usage");

        auto result = options.parse(static_cast<int32_t>(arguments.args.size()), arguments.args.data());

        if (result.count("help") != 0u) {
            printf("%s", options.help().c_str());
            return;
        }
        if (result.count("project") != 0u) {
            projectPath = result["project"].as<std::string>();
        }
        if (result.count("module") != 0u) {
            auto modules = result["module"].as<std::vector<std::string>>();
            for (auto &module : modules) {
                moduleManager->RegisterModule({module, {}});
            }
        }
    }

    void ToolApplicationBase::LoadConfigs()
    {
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
    }

    void ToolApplicationBase::PostInit()
    {
        AssetManager::Get()->SetProjectPath(projectPath);
    }
#endif
} // namespace sky