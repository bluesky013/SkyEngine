//
// Created by blues on 2023/12/28.
//

#include <framework/application/ModuleManager.h>

#include <core/logger/Logger.h>

static const char *TAG = "ModuleManager";

namespace sky {

    using ModuleStart = IModule *(*)(Environment *);
    using ModuleStop  = void (*)();

    void ModuleManager::RegisterModule(const ModuleInfo &info)
    {
        auto src = RegisterModuleImpl(info.name);
        for (const auto &dep : info.dependencies) {
            auto dst = RegisterModuleImpl(dep);
            dependencyGraph.AddEdge(src, dst);
        }
    }

    ModuleManager::vertex_descriptor ModuleManager::RegisterModuleImpl(const std::string &str)
    {
        auto iter = std::find(names.begin(), names.end(), str);
        if (iter != names.end()) {
            return std::distance(names.begin(), iter);
        }
        auto vtx = dependencyGraph.AddVertex();
        if (vtx >= names.size()) {
            names.resize(vtx + 1);
            names[vtx] = str;
        }
        return vtx;
    }


    void ModuleManager::TopoSort()
    {
        sortedContainer = dependencyGraph.TopologicalSort();
    }

    void ModuleManager::Tick(float time)
    {
        WalkModules([this, time](const std::string &moduleName) {
            modules[moduleName]->PreTick(time);
        }, sortedContainer);

        WalkModules([this, time](const std::string &moduleName) {
            modules[moduleName]->Tick(time);
        }, sortedContainer);

        WalkModules([this, time](const std::string &moduleName) {
            modules[moduleName]->PostTick(time);
        }, sortedContainer);
    }

    void ModuleManager::LoadModules(const StartArguments &args)
    {
        WalkModules([this, &args](const std::string &moduleName) {
            auto dynModule = std::make_unique<DynamicModule>(moduleName);
            LOG_I(TAG, "Load Module : %s", moduleName.c_str());
            if (dynModule->Load()) {
                auto startFn = dynModule->GetAddress<ModuleStart>("StartModule");
                if (startFn == nullptr) {
                    LOG_E(TAG, "Load Module : %s failed", moduleName.c_str());
                    return;
                }
                auto *module = startFn(Environment::Get());
                if (module == nullptr || !module->Init(args)) {
                    return;
                }

                modules.emplace(moduleName, std::unique_ptr<IModule>(module));
                dynLibs.emplace(moduleName, std::move(dynModule));
                LOG_I(TAG, "Load Module : %s success", moduleName.c_str());
            } else {
                LOG_E(TAG, "Load Module : %s Failed, [%s]", moduleName.c_str(), dynModule->GetLastError().c_str());
            }
        }, sortedContainer);
    }

    void ModuleManager::StartModules()
    {
        WalkModules([this](const std::string &moduleName) {
            modules[moduleName]->Start();
        });
    }

    void ModuleManager::UnLoadModules()
    {
        if (sortedContainer.empty()) {
            TopoSort();
        }
        std::vector<vertex_descriptor> container(sortedContainer.rbegin(), sortedContainer.rend());

        WalkModules([this](const std::string &moduleName) {
            modules[moduleName]->Shutdown();
        }, container);
        modules.clear();

        for (auto &[key, lib] : dynLibs) {
            auto stopFn = lib->GetAddress<ModuleStop>("StopModule");
            if (stopFn != nullptr) {
                stopFn();
            }
        }
    }

} // namespace sky
