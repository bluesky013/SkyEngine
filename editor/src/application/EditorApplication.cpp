//
// Created by Zach Lee on 2021/12/12.
//

#include <editor/application/EditorApplication.h>
#include <editor/document/Document.h>
#include <core/environment/Environment.h>
#include <core/logger/Logger.h>
#include <framework/platform/PlatformBase.h>
#include <framework/asset/AssetManager.h>
#include <framework/asset/AssetDataBase.h>

#include <cxxopts.hpp>

static const char* TAG = "EditorApplication";

namespace sky::editor {

    static void EditorReflect()
    {
        Document::Reflect();
    }

    EditorApplication::EditorApplication() = default;

    EditorApplication::~EditorApplication() // NOLINT
    {
    }

    bool EditorApplication::Init(int argc, char **argv)
    {
        cxxopts::Options options("GameApplication Launcher", "SkyEngine Launcher");
        options.allow_unrecognised_options();

        options.add_options()("p,project", "Project Directory", cxxopts::value<std::string>());
        options.add_options()("e,engine", "Engine Directory", cxxopts::value<std::string>());
        auto result = options.parse(argc, argv);
        if (result.count("project") == 0u || result.count("engine") == 0u) {
            return false;
        }
        std::string projectPath = result["project"].as<std::string>();
        std::string enginePath = result["engine"].as<std::string>();

        workFs = new NativeFileSystem(projectPath);
        engineFs = new NativeFileSystem(enginePath);

        if (!Application::Init(argc, argv)) {
            return false;
        }


        AssetManager::Get()->SetWorkFileSystem(workFs);

        AssetDataBase::Get()->SetEngineFs(engineFs);
        AssetDataBase::Get()->SetWorkSpaceFs(workFs);

        EditorReflect();

        BindTick([this](float delta) {
//            engine->Tick(delta);
        });

        timer = new QTimer(this);
        timer->start(0);
        connect(timer, &QTimer::timeout, this, [this]() {
            Loop();
        });

        return true;
    }

    void EditorApplication::LoadConfigs()
    {
        std::string json;
        auto file = workFs->OpenFile("config/modules_editor.json");
        if (!file || !file->ReadString(json)) {
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
}
