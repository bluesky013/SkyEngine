//
// Created by blues on 2023/12/28.
//

#include <framework/application/ModuleManager.h>

#include <core/environment/Once.h>
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

    static IModule *FindModule(const std::unordered_map<std::string, std::unique_ptr<IModule>> &modules, const std::string &name)
    {
        const auto iter = modules.find(name);
        return iter == modules.end() ? nullptr : iter->second.get();
    }

    // Reflection/asset-type registration is process-global and persists across unload, so it must
    // run at most once per process per module - never once per load (that is what caused duplicate
    // registrations on reload / across module DLLs).
    static void RegisterMetaOnce(IModule &module, const std::string &name)
    {
        const std::string key = "ModuleMeta:" + name;
        if (Once::Get()->Run(key.c_str())) {
            module.RegisterMeta();
        }
    }

    void ModuleManager::Tick(float time)
    {
        WalkModules(
            [this, time](const std::string &moduleName) {
                if (auto *module = FindModule(modules, moduleName)) {
                    module->PreTick(time);
                }
            },
            sortedContainer);

        WalkModules(
            [this, time](const std::string &moduleName) {
                if (auto *module = FindModule(modules, moduleName)) {
                    module->Tick(time);
                }
            },
            sortedContainer);

        WalkModules(
            [this, time](const std::string &moduleName) {
                if (auto *module = FindModule(modules, moduleName)) {
                    module->PostTick(time);
                }
            },
            sortedContainer);
    }

    void ModuleManager::AddStaticModule(const std::string &id, std::unique_ptr<IModule> module)
    {
        // Duplicate static registration (e.g. the same module registered by two
        // entries): keep the first deterministically and report it.
        if (modules.find(id) != modules.end()) {
            LOG_W(TAG, "Static module '%s' already registered; ignoring duplicate", id.c_str());
            return;
        }
        // Ensure the id participates in the load/start order (same graph as dynamic).
        RegisterModule(ModuleInfo{id, {}});
        modules.emplace(id, std::move(module));
    }

    void ModuleManager::LoadModules(const StartArguments &args)
    {
        WalkModules(
            [this, &args](const std::string &moduleName) {
                // Statically bundled (compiled-in registration): no dlopen, just Init.
                if (auto staticIt = modules.find(moduleName); staticIt != modules.end()) {
                    LOG_I(TAG, "Load Module (static) : %s", moduleName.c_str());
                    RegisterMetaOnce(*staticIt->second, moduleName);
                    if (!staticIt->second->Init(args)) {
                        LOG_E(TAG, "Load Module (static) : %s failed", moduleName.c_str());
                    }
                    return;
                }

                auto dynModule = std::make_unique<DynamicModule>(moduleName);
                LOG_I(TAG, "Load Module : %s", moduleName.c_str());
                if (dynModule->Load()) {
                    auto startFn = dynModule->GetAddress<ModuleStart>("StartModule");
                    if (startFn == nullptr) {
                        LOG_E(TAG, "Load Module : %s failed", moduleName.c_str());
                        return;
                    }
                    auto *module = startFn(Environment::Get());
                    if (module == nullptr) {
                        return;
                    }
                    RegisterMetaOnce(*module, moduleName);
                    if (!module->Init(args)) {
                        return;
                    }

                    modules.emplace(moduleName, std::unique_ptr<IModule>(module));
                    dynLibs.emplace(moduleName, std::move(dynModule));
                    LOG_I(TAG, "Load Module : %s success", moduleName.c_str());
                } else {
#if defined(SKY_MODULE_STATIC)
                    // Static packaging: a missing module means it was not compiled in.
                    LOG_E(TAG,
                          "Load Module : '%s' Failed, [%s] - not statically registered (declare its target via "
                          "sky_add_module + REGISTER_MODULE_TAGGED and list it in the runtime module config)",
                          moduleName.c_str(), dynModule->GetLastError().c_str());
#else
                    LOG_E(TAG, "Load Module : %s Failed, [%s]", moduleName.c_str(), dynModule->GetLastError().c_str());
#endif
                }
            },
            sortedContainer);
    }

    void ModuleManager::StartModules()
    {
        WalkModules([this](const std::string &moduleName) {
            if (auto *module = FindModule(modules, moduleName)) {
                module->Start();
            }
        });
    }

    void ModuleManager::UnLoadModules()
    {
        if (sortedContainer.empty()) {
            TopoSort();
        }
        std::vector<vertex_descriptor> container(sortedContainer.rbegin(), sortedContainer.rend());

        WalkModules(
            [this](const std::string &moduleName) {
                if (auto *module = FindModule(modules, moduleName)) {
                    module->Shutdown();
                }
            },
            container);
        modules.clear();

        for (auto &[key, lib] : dynLibs) {
            auto stopFn = lib->GetAddress<ModuleStop>("StopModule");
            if (stopFn != nullptr) {
                stopFn();
            }
        }
    }

} // namespace sky
