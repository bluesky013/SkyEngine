//
// Created by blues on 2023/12/28.
//

#include <framework/application/ModuleManager.h>

#include <boost/graph/topological_sort.hpp>

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
            boost::add_edge(src, dst, dependencyGraph);
        }
    }

    ModuleManager::vertex_descriptor ModuleManager::RegisterModuleImpl(const std::string &str)
    {
        auto iter = std::find(names.begin(), names.end(), str);
        if (iter != names.end()) {
            return std::distance(names.begin(), iter);
        }
        auto vtx = boost::add_vertex(dependencyGraph);
        if (vtx >= names.size()) {
            names.resize(vtx + 1);
            names[vtx] = str;
        }
        return vtx;
    }


    void ModuleManager::TopoSort()
    {
        boost::topological_sort(dependencyGraph, std::back_inserter(sortedContainer));
    }

    void ModuleManager::Tick(float time)
    {
        WalkModules([this, time](const std::string &moduleName) {
            modules[moduleName]->Tick(time);
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
                dynLibs.emplace_back(std::move(dynModule));
            }
            LOG_I(TAG, "Load Module : %s success", moduleName.c_str());
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
        std::list<vertex_descriptor> container;
        boost::topological_sort(dependencyGraph, std::front_inserter(container));

        WalkModules([this](const std::string &moduleName) {
            modules[moduleName]->Shutdown();
        }, container);
        modules.clear();

        for (auto &lib : dynLibs) {
            auto stopFn = lib->GetAddress<ModuleStop>("StopModule");
            if (stopFn != nullptr) {
                stopFn();
            }
        }
    }

} // namespace sky