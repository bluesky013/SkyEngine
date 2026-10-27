//
// Created by blues on 2026/10/3.
//

#include "CookWorkerHost.h"
#include "ProductAudit.h"

#include <core/cmdline/CmdParser.h>
#include <core/file/MultiFileSystem.h>
#include <core/logger/Logger.h>
#include <framework/application/Application.h>
#include <framework/asset/AssetBuilderManager.h>
#include <framework/asset/AssetDataBase.h>
#include <framework/asset/AssetExecutor.h>
#include <framework/asset/AssetManager.h>
#include <framework/platform/PlatformBase.h>

#include <chrono>
#include <cstdio>
#include <future>
#include <string>
#include <vector>

using namespace sky;

namespace {

    const char *TAG = "AssetTool";

    // Headless worker application: replicates the editor's asset bootstrap (mounts,
    // source catalog, builder filesystems) and loads the builder modules (D10).
    class WorkerApplication : public Application {
    public:
        // True when invoked as a one-shot identity migration (no IPC host loop).
        bool MigrateRequested() const
        {
            return migrate;
        }

        // True when invoked as a one-shot batch cook (no IPC host loop).
        bool CookAllRequested() const
        {
            return cookAll;
        }

        const std::string &CookPlatform() const
        {
            return platform;
        }
        const std::string &AuditManifest() const
        {
            return auditManifest;
        }

    protected:
        void ParseStartArgs() override
        {
            CmdOptions options("AssetTool", "SkyEngine asset cook worker");
            options.allow_unrecognised_options();
            options.add_options()("audit-products", "Verify a staged package asset-roots.json without source access or cook",
                                  CmdValue<std::string>());
            options.add_options()("p,project", "Project Directory", CmdValue<std::string>())("e,engine", "Engine Directory", CmdValue<std::string>())(
                "i,intermediate", "Intermediate Directory",
                CmdValue<std::string>())("m,migrate", "One-shot: seed legacy (bundle, path) UUIDs into every source manifest, then exit")(
                "cook-all", "One-shot: cook all project sources for --platform, then exit")("platform", "Target platform for --cook-all",
                                                                                            CmdValue<std::string>())("h,help", "Print usage");

            // Scripts invoke this application with argv[1..] only; cxxopts still
            // expects a program name in slot 0.
            std::vector<const char *> argv(arguments.args.begin(), arguments.args.end());
            if (argv.empty() || (argv.front() != nullptr && argv.front()[0] == '-')) {
                argv.insert(argv.begin(), "AssetTool");
            }
            auto result = options.parse(static_cast<int32_t>(argv.size()), argv.data());
            if (result.count("project") != 0u) {
                projectPath = result["project"].as<std::string>();
            }
            if (result.count("engine") != 0u) {
                enginePath = result["engine"].as<std::string>();
            }
            if (result.count("intermediate") != 0u) {
                intermediatePath = result["intermediate"].as<std::string>();
            }
            if (result.count("platform") != 0u) {
                platform = result["platform"].as<std::string>();
            }
            migrate = result.count("migrate") != 0u;
            cookAll = result.count("cook-all") != 0u;
            if (result.count("audit-products") != 0u)
                auditManifest = result["audit-products"].as<std::string>();
        }

        bool LoadConfigs() override
        {
            if (!auditManifest.empty()) {
                if (moduleManager && ModuleManager::GetBuildOption("SKY_BUILD_COMPRESSION").value_or(false))
                    moduleManager->RegisterModule(ModuleInfo{"CompressionModule", {}});
                return true;
            }
            if (moduleManager) {
                // Builder modules register the concrete AssetBuilders; missing DLLs are
                // skipped by ModuleManager and cooks then fail observably.
                moduleManager->RegisterModule(ModuleInfo{"Aurora.Cook", {"Aurora.Metadata"}});
                moduleManager->RegisterModule(ModuleInfo{"SkyAudio.Builder", {}});
#if SKY_HAS_JOLT_BUILDER
                moduleManager->RegisterModule(ModuleInfo{"JoltPhysics.Builder", {}});
#endif
                moduleManager->RegisterModule(ModuleInfo{"SkyNavigation.Builder", {}});
                moduleManager->RegisterModule(ModuleInfo{"SkyTerrain.Builder", {}});
                moduleManager->RegisterModule(ModuleInfo{"Content.Builder.Module", {"Content.Metadata"}});
#if SKY_HAS_ANIMATION_BUILDER
                moduleManager->RegisterModule(ModuleInfo{"Animation.Builder", {"Animation.GraphAssets"}});
#endif
            }
            return true;
        }

        bool PreInit() override
        {
            if (!auditManifest.empty())
                return true;
            if (projectPath.empty() || enginePath.empty()) {
                LOG_E(TAG, "worker requires --project and --engine");
                return false;
            }

            const std::string intermediate = intermediatePath.empty() ? (projectPath + "/Intermediate") : intermediatePath;

            const NativeFileSystemPtr workFs   = new NativeFileSystem(projectPath);
            const NativeFileSystemPtr engineFs = new NativeFileSystem(enginePath);

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

        void PostInit() override
        {
            if (!auditManifest.empty())
                return;
            // Builders are registered by the modules after PreInit. Load their
            // settings now, with project files overriding deployed engine defaults.
            auto                       *builders = AssetBuilderManager::Get();
            CounterPtr<MultiFileSystem> configs  = new MultiFileSystem();
            configs->AddFileSystem(builders->GetWorkSpaceFs()->CreateSubSystem("configs", false));
            configs->AddFileSystem(builders->GetEngineFs()->CreateSubSystem("configs", false));
            builders->LoadBuildConfigs(configs);
        }

    private:
        std::string projectPath;
        std::string enginePath;
        std::string intermediatePath;
        std::string platform;
        std::string auditManifest;
        bool        migrate = false;
        bool        cookAll = false;
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

    if (!app.AuditManifest().empty()) {
        const auto status = AuditPackagedProducts(app.AuditManifest());
        if (!status)
            LOG_E(TAG, "package audit failed: %s", status.message.c_str());
        else
            LOG_I(TAG, "package dependency closure verified without source access");
        return status ? 0 : 1;
    }

    // Populate the source catalog (needs builders registered by the modules above).
    if (const auto error = AssetDataBase::Get()->Load()) {
        LOG_E(TAG, "asset scan failed at '%s': %s", error->path.GetStr().c_str(), error->code.message().c_str());
        return 1;
    }

    // One-shot identity migration (`--migrate`): seed legacy UUIDs, then exit (no IPC host loop).
    if (app.MigrateRequested()) {
        if (const auto error = AssetDataBase::Get()->MigrateLegacyIdentity()) {
            LOG_E(TAG, "identity migration scan failed at '%s': %s", error->path.GetStr().c_str(), error->code.message().c_str());
            return 1;
        }
        LOG_I(TAG, "legacy identity migration complete");
        return 0;
    }

    // One-shot batch cook (`--cook-all --platform <p>`): cook every source for the
    // platform's targets into <project>/products, then exit (no IPC host loop).
    if (app.CookAllRequested()) {
        auto *builderMgr = AssetBuilderManager::Get();
        auto  cookConfig = builderMgr->GetCookConfig();
        if (!app.CookPlatform().empty()) {
            cookConfig.SetActivePlatform(app.CookPlatform());
        }
        builderMgr->SetCookConfig(std::move(cookConfig));
        const std::string activePlatform = builderMgr->GetCookConfig().GetActivePlatform();

        uint32_t cooked = 0;
        uint32_t failed = 0;
        AssetDataBase::Get()->ForEachSource([&](const AssetSourcePtr &src) {
            // World manifests are read by World::LoadJson; their asset roots are cooked products.
            if (src->ext == ".world")
                return;
            const std::string cookJson = AssetDataBase::Get()->GetCookJson(src->uuid);
            for (const auto &target : builderMgr->GetCookConfig().GetTargets(cookJson, activePlatform)) {
                std::promise<AssetBuildResult> completion;
                auto                           result = completion.get_future();
                builderMgr->BuildRequest(src->uuid, target, [&completion](const AssetBuildResult &value) { completion.set_value(value); });
                AssetExecutor::Get()->WaitForAll();
                if (result.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
                    LOG_E(TAG, "cook-all: no build result for '%s' target '%s'", src->path.GetStr().c_str(), target.c_str());
                    ++failed;
                    continue;
                }
                const auto build = result.get();
                if (build.retCode != AssetBuildRetCode::SUCCESS) {
                    LOG_E(TAG, "cook-all: failed '%s' target '%s': %s", src->path.GetStr().c_str(), target.c_str(), build.error.c_str());
                    ++failed;
                    continue;
                }
                ++cooked;
            }
        });
        LOG_I(TAG, "cook-all: %u product(s), %u failed for platform '%s'", cooked, failed, activePlatform.c_str());
        return failed == 0 ? 0 : 1;
    }

    CookWorkerHost host;
    return host.Run() ? 0 : -1;
}
