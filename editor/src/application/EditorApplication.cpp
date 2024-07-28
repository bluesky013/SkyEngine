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
#include <framework/interface/ITickEvent.h>

#include <cxxopts.hpp>

static const char* TAG = "EditorApplication";

namespace sky::editor {

    static void EditorReflect()
    {
        Document::Reflect();
    }

    EditorApplication::EditorApplication(int argc, char **argv) : QApplication(argc, argv)
    {
    }

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

        AssetManager::Get()->SetWorkFileSystem(workFs);
        AssetDataBase::Get()->SetEngineFs(engineFs);
        AssetDataBase::Get()->SetWorkSpaceFs(workFs);

        SplashWindow();

        if (!Application::Init(argc, argv)) {
            return false;
        }

        EditorReflect();

        BindTick([this](float delta) {

            TickEvent::BroadCast(&ITickEvent::Tick, delta);
            auto world = mainWindow->GetDoc()->GetWorld();
            if (world) {
                world->Tick(delta);
            }
//            engine->Tick(delta);
        });

        timer = new QTimer(this);
        timer->start(0);
        connect(timer, &QTimer::timeout, this, [this]() {
            Loop();
        });
        return true;
    }

    void EditorApplication::SplashWindow()
    {
        auto splashPath =  engineFs->GetPath();
        splashPath /= "assets/splash/test.png";

        QPixmap pixmap(splashPath.GetStr().c_str());
        QSplashScreen splash(pixmap);
        splash.show();

        splash.showMessage("loadModules", Qt::AlignBottom, Qt::white);
        std::this_thread::sleep_for(std::chrono::duration<float, std::milli>(200));
        processEvents();

        splash.showMessage("loadAssets", Qt::AlignBottom, Qt::white);
        std::this_thread::sleep_for(std::chrono::duration<float, std::milli>(200));
        processEvents();

        mainWindow = std::make_unique<MainWindow>();
        mainWindow->show();
        splash.finish(mainWindow.get());
    }

    void EditorApplication::LoadFromJson(std::unordered_map<std::string, ModuleInfo> &modules)
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

                modules.emplace(info.name, std::move(info));
            }
        }
    }

    void EditorApplication::LoadConfigs()
    {
        std::unordered_map<std::string, ModuleInfo> modules = {};
        modules.emplace("SkyRender", ModuleInfo{"SkyRender", {"ShaderCompiler"}});
        modules.emplace("RenderBuilder", ModuleInfo{"RenderBuilder", {"SkyRender"}});

        LoadFromJson(modules);

        for (auto &[key, info] : modules) {
            moduleManager->RegisterModule(info);
        }
    }
}
