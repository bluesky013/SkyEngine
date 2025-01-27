//
// Created by Zach Lee on 2021/12/12.
//

#include <editor/application/EditorApplication.h>
#include <editor/document/Document.h>
#include <editor/framework/ViewportWidget.h>
#include <editor/framework/SelectTool.h>
#include <editor/framework/AssetCreator.h>
#include <editor/framework/render/MaterialCreator.h>
#include <core/environment/Environment.h>
#include <core/logger/Logger.h>
#include <framework/platform/PlatformBase.h>
#include <framework/asset/AssetManager.h>
#include <framework/asset/AssetDataBase.h>
#include <framework/asset/AssetBuilderManager.h>
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
        if (timer != nullptr) {
            timer->stop();
        }
    }

    bool EditorApplication::Init(int argc, char **argv)
    {
        cxxopts::Options options("GameApplication Launcher", "SkyEngine Launcher");
        options.allow_unrecognised_options();

        options.add_options()("p,project", "Project Directory", cxxopts::value<std::string>());
        options.add_options()("e,engine", "Engine Directory", cxxopts::value<std::string>());
        options.add_options()("i,intermediate", "Project Intermediate Directory", cxxopts::value<std::string>());
        auto result = options.parse(argc, argv);
        if (result.count("project") == 0u || result.count("engine") == 0u) {
            return false;
        }
        std::string projectPath = result["project"].as<std::string>();
        std::string enginePath = result["engine"].as<std::string>();

        FilePath intermediatePath;
        if (result.count("intermediate") != 0u) {
            intermediatePath = FilePath(result["intermediate"].as<std::string>());
        } else {
            intermediatePath = FilePath(projectPath) / FilePath("Intermediate");
        }

        workFs = new NativeFileSystem(projectPath);
        engineFs = new NativeFileSystem(enginePath);

        AssetManager::Get()->SetWorkFileSystem(workFs);
        AssetDataBase::Get()->SetEngineFs(engineFs);
        AssetDataBase::Get()->SetWorkSpaceFs(workFs);

        AssetBuilderManager::Get()->SetEngineFs(engineFs);
        AssetBuilderManager::Get()->SetWorkSpaceFs(workFs);
        AssetBuilderManager::Get()->SetInterMediateFs(new NativeFileSystem(intermediatePath));

        EditorToolManager::Get()->RegisterTool(Name("Select"), new SelectTool());

        AssetCreatorManager::Get()->RegisterTool(Name("Material"), new MaterialInstanceCreator());

        if (!InitAppAndSplashWindow(argc, argv)) {
            return false;
        }

        EditorReflect();

        BindTick([this](float delta) {

            TickEvent::BroadCast(&ITickEvent::Tick, delta);
            const auto &world = mainWindow->GetDoc()->GetWorld();
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

    void EditorApplication::InitTools()
    {

    }

    bool EditorApplication::InitAppAndSplashWindow(int argc, char **argv)
    {
        auto splashPath =  engineFs->GetPath();
        splashPath /= "assets/splash/splash.jpg";

        QPixmap pixmap(splashPath.GetStr().c_str());
        QSplashScreen splash(pixmap);
        splash.show();

        splash.showMessage("loadModules", Qt::AlignBottom, Qt::white);
        std::this_thread::sleep_for(std::chrono::duration<float, std::milli>(200));
        processEvents();

        splash.showMessage("loadAssets", Qt::AlignBottom, Qt::white);
        std::this_thread::sleep_for(std::chrono::duration<float, std::milli>(200));
        processEvents();

        if (!Application::Init(argc, argv)) {
            return false;
        }
        InitTools();

        mainWindow = std::make_unique<MainWindow>();
        mainWindow->show();
        splash.finish(mainWindow.get());

        return true;
    }

    void EditorApplication::LoadFromJson(std::unordered_map<std::string, ModuleInfo> &modules)
    {
        std::string json;
        auto file = workFs->OpenFile("configs/modules_editor.json");
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
