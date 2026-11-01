//
// Created by blues on 2023/12/28.
//

#include <framework/application/ModuleManager.h>

#include "BuildPluginOptions.h"
#include <core/environment/Singleton.h>
#include <core/logger/Logger.h>
#include <framework/serialization/SerializationContext.h>

static const char *TAG = "ModuleManager";

namespace sky {

    namespace {
        struct ModuleDefinition {
            ModuleDefinition(ModuleInfo info, IModule &module) : info(std::move(info))
            {
                module.RegisterMeta();
            }
            ModuleInfo info;
        };

        class ModuleDefinitions : public Singleton<ModuleDefinitions> {
        public:
            const ModuleDefinition &Resolve(ModuleInfo info, IModule &module)
            {
                if (const auto found = definitions.find(info.name); found != definitions.end()) {
                    return found->second;
                }
                const auto name              = info.name;
                const auto [entry, inserted] = definitions.try_emplace(name, std::move(info), module);
                SKY_ASSERT(inserted)
                return entry->second;
            }

        private:
            friend class Singleton<ModuleDefinitions>;
            ModuleDefinitions() = default;
            std::unordered_map<std::string, ModuleDefinition> definitions;
        };
    } // namespace

    ModuleManager::~ModuleManager()
    {
        UnLoadModules();
    }

    std::optional<bool> ModuleManager::GetBuildOption(std::string_view option)
    {
        for (const auto &[name, enabled] : build::PLUGIN_OPTIONS)
            if (name == option)
                return enabled;
        return std::nullopt;
    }

    using ModuleStart = IModule *(*)(Environment *);

    bool ModuleManager::RegisterModule(const ModuleInfo &info)
    {
        if (phase != Phase::Idle) {
            LOG_E(TAG, "Cannot register modules during lifecycle callbacks");
            return false;
        }
        auto  src      = RegisterModuleImpl(info.name);
        auto &required = dependencies[info.name];
        for (const auto &dep : info.dependencies)
            if (std::find(required.begin(), required.end(), dep) == required.end())
                required.push_back(dep);
        for (const auto &dep : info.dependencies) {
            auto dst = RegisterModuleImpl(dep);
            dependencyGraph.AddEdge(src, dst);
        }
        return true;
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

    bool ModuleManager::TopoSort()
    {
        if (phase != Phase::Idle)
            return false;
        sortedContainer = dependencyGraph.TopologicalSort();
        return true;
    }

    static IModule *FindModule(const std::unordered_map<std::string, std::unique_ptr<IModule>> &modules, const std::string &name)
    {
        const auto iter = modules.find(name);
        return iter == modules.end() ? nullptr : iter->second.get();
    }

    bool ModuleManager::Tick(float time)
    {
        if (phase != Phase::Idle)
            return false;
        phase = Phase::Dispatching;
        WalkModules(
            [this, time](const std::string &moduleName) {
                if (auto *module = FindModule(modules, moduleName); module && IsModuleInitialized(moduleName)) {
                    module->PreTick(time);
                }
            },
            sortedContainer);

        WalkModules(
            [this, time](const std::string &moduleName) {
                if (auto *module = FindModule(modules, moduleName); module && IsModuleInitialized(moduleName)) {
                    module->Tick(time);
                }
            },
            sortedContainer);

        WalkModules(
            [this, time](const std::string &moduleName) {
                if (auto *module = FindModule(modules, moduleName); module && IsModuleInitialized(moduleName)) {
                    module->PostTick(time);
                }
            },
            sortedContainer);
        phase = Phase::Idle;
        return true;
    }

    bool ModuleManager::AddStaticModule(const std::string &id, std::unique_ptr<IModule> module)
    {
        if (phase != Phase::Idle || !module) {
            LOG_E(TAG, "Cannot add an empty static module or register during lifecycle callbacks");
            return false;
        }
        // Duplicate static registration (e.g. the same module registered by two
        // entries): keep the first deterministically and report it.
        if (modules.find(id) != modules.end()) {
            LOG_W(TAG, "Static module '%s' already registered; ignoring duplicate", id.c_str());
            return false;
        }
        // Ensure the id participates in the load/start order (same graph as dynamic).
        RegisterModule(ModuleInfo{id, {}});
        modules.emplace(id, std::move(module));
        return true;
    }

    bool ModuleManager::IsModuleInitialized(const std::string &name) const
    {
        return initializedModules.contains(name);
    }
    bool ModuleManager::LoadModules(const StartArguments &args)
    {
        if (phase != Phase::Idle)
            return false;
        phase          = Phase::Initializing;
        bool succeeded = true;
        WalkModules(
            [this, &args, &succeeded](const std::string &moduleName) {
                if (IsModuleInitialized(moduleName))
                    return;
                for (const auto &dependency : dependencies[moduleName]) {
                    if (!IsModuleInitialized(dependency)) {
                        LOG_E(TAG, "Module '%s' requires loaded '%s'", moduleName.c_str(), dependency.c_str());
                        succeeded = false;
                        modules.erase(moduleName);
                        return;
                    }
                }
                // Statically bundled (compiled-in registration): no dlopen, just Init.
                if (auto staticIt = modules.find(moduleName); staticIt != modules.end()) {
                    LOG_I(TAG, "Load Module (static) : %s", moduleName.c_str());
                    ModuleDefinitions::Get()->Resolve({moduleName, dependencies[moduleName]}, *staticIt->second);
                    if (!staticIt->second->Init(args)) {
                        succeeded = false;
                        LOG_E(TAG, "Load Module (static) : %s failed", moduleName.c_str());
                        staticIt->second->Shutdown();
                        modules.erase(staticIt);
                    } else {
                        initializedModules.insert(moduleName);
                    }
                    return;
                }

                auto dynModule = std::make_unique<DynamicModule>(moduleName);
                LOG_I(TAG, "Load Module : %s", moduleName.c_str());
                if (dynModule->IsLoaded() || dynModule->Load()) {
                    auto startFn = dynModule->GetAddress<ModuleStart>("StartModule");
                    if (startFn == nullptr) {
                        succeeded = false;
                        LOG_E(TAG, "Load Module : %s failed", moduleName.c_str());
                        return;
                    }
                    dynLibs[moduleName] = std::move(dynModule);
                    auto module         = std::unique_ptr<IModule>(startFn(Environment::Get()));
                    if (module)
                        ModuleDefinitions::Get()->Resolve({moduleName, dependencies[moduleName]}, *module);
                    if (module == nullptr || !module->Init(args)) {
                        succeeded = false;
                        if (module) {
                            module->Shutdown();
                            module.reset();
                        }
                        dynLibs.erase(moduleName);
                        LOG_E(TAG, "Load Module : %s initialization failed", moduleName.c_str());
                        return;
                    }

                    modules.emplace(moduleName, std::move(module));
                    initializedModules.insert(moduleName);

                    LOG_I(TAG, "Load Module : %s success", moduleName.c_str());
                } else {
                    succeeded = false;
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
        phase = Phase::Idle;
        return succeeded;
    }

    bool ModuleManager::StartModules()
    {
        if (phase != Phase::Idle)
            return false;
        phase = Phase::Dispatching;
        WalkModules([this](const std::string &moduleName) {
            if (auto *module = FindModule(modules, moduleName); module && IsModuleInitialized(moduleName)) {
                module->Start();
            }
        });
        phase = Phase::Idle;
        return true;
    }

    bool ModuleManager::UnLoadModules()
    {
        if (phase == Phase::Initializing || phase == Phase::Dispatching)
            return false;
        if (phase == Phase::Stopping)
            return true;
        if (sortedContainer.empty()) {
            TopoSort();
        }
        phase = Phase::Stopping;
        std::vector<vertex_descriptor> container(sortedContainer.rbegin(), sortedContainer.rend());

        WalkModules(
            [this](const std::string &moduleName) {
                if (const auto found = modules.find(moduleName); found != modules.end()) {
                    auto module = std::move(found->second);
                    modules.erase(found);
                    if (initializedModules.erase(moduleName))
                        module->Shutdown();
                }
            },
            container);
        modules.clear();

        initializedModules.clear();
        dynLibs.clear();
        phase = Phase::Idle;
        return true;
    }

} // namespace sky
