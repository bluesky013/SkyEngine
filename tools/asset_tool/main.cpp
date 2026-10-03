//
// Created by blues on 2026/10/3.
//

#include "CookWorkerHost.h"

#include <framework/application/Application.h>
#include <framework/asset/AssetBuilderManager.h>
#include <framework/asset/AssetDataBase.h>
#include <framework/asset/AssetManager.h>
#include <framework/platform/PlatformBase.h>
#include <core/cmdline/CmdParser.h>
#include <core/logger/Logger.h>

#include <cstdio>
#include <string>

using namespace sky;

namespace {

    const char *TAG = "AssetTool";

    // Headless worker application: replicates the editor's asset bootstrap (mounts,
    // source catalog, builder filesystems) and loads the builder modules (D10).
    class WorkerApplication : public Application {
    protected:
        void ParseStartArgs() override
        {
            CmdOptions options("AssetTool", "SkyEngine asset cook worker");
            options.allow_unrecognised_options();
            options.add_options()
                ("p,project", "Project Directory", CmdValue<std::string>())
                ("e,engine", "Engine Directory", CmdValue<std::string>())
                ("i,intermediate", "Intermediate Directory", CmdValue<std::string>())
                ("h,help", "Print usage");

            auto result = options.parse(static_cast<int32_t>(arguments.args.size()), arguments.args.data());
            if (result.count("project") != 0u) {
                projectPath = result["project"].as<std::string>();
            }
            if (result.count("engine") != 0u) {
                enginePath = result["engine"].as<std::string>();
            }
            if (result.count("intermediate") != 0u) {
                intermediatePath = result["intermediate"].as<std::string>();
            }
        }

        bool LoadConfigs() override
        {
            if (moduleManager) {
                // Builder modules register the concrete AssetBuilders; missing DLLs are
                // skipped by ModuleManager and cooks then fail observably.
                moduleManager->RegisterModule(ModuleInfo{"SkyRender.Builder", {}});
                moduleManager->RegisterModule(ModuleInfo{"SkyAudio.Builder", {}});
                moduleManager->RegisterModule(ModuleInfo{"SkyNavigation.Builder", {}});
            }
            return true;
        }

        bool PreInit() override
        {
            if (projectPath.empty() || enginePath.empty()) {
                LOG_E(TAG, "worker requires --project and --engine");
                return false;
            }

            const std::string intermediate = intermediatePath.empty() ? (projectPath + "/Intermediate") : intermediatePath;

            auto workFs = new NativeFileSystem(projectPath);
            auto engineFs = new NativeFileSystem(enginePath);

            AssetManager::Get()->SetWorkFileSystem(workFs);
            AssetDataBase::Get()->SetEngineFs(engineFs);
            AssetDataBase::Get()->SetWorkSpaceFs(workFs->CreateSubSystem("assets", true));
            AssetManager::Get()->SetSourceCatalog(AssetDataBase::Get());

            // The worker always cooks in-process (it IS the worker); never recurse into
            // an out-of-process runner, and satisfy the single-writer invariant.
            AssetBuilderManager::Get()->SetForceInProcess(true);

            AssetBuilderManager::Get()->SetEngineFs(engineFs);
            AssetBuilderManager::Get()->SetWorkSpaceFs(workFs);
            AssetBuilderManager::Get()->SetInterMediateFs(new NativeFileSystem(intermediate));

            AssetManager::Get()->SetCookRunner(nullptr);
            return true;
        }

    private:
        std::string projectPath;
        std::string enginePath;
        std::string intermediatePath;
    };

} // namespace

int main(int argc, char **argv)
{
    // Route engine logs off stdout before any engine init; frames use the preserved fd.
    Logger::SetOutputStream(stderr);

    Platform *platform = Platform::Get();
    if (!platform->Init({})) {
        return -1;
    }

    WorkerApplication app;
    if (!app.Init(argc, argv)) {
        return -1;
    }

    // Populate the source catalog (needs builders registered by the modules above).
    AssetDataBase::Get()->Load();

    CookWorkerHost host;
    return host.Run() ? 0 : -1;
}
