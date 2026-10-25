//
// Created by blues on 2023/12/28.
//

#include <framework/application/ModuleManager.h>

#include <core/environment/Singleton.h>
#include <core/logger/Logger.h>
#include <framework/serialization/SerializationContext.h>

static const char *TAG = "ModuleManager";

namespace sky {

    namespace {
        class CurrentManager : public Singleton<CurrentManager> {
        public:
            ModuleManager *value = nullptr;

        private:
            friend class Singleton<CurrentManager>;
            CurrentManager() = default;
        };

        struct ModuleDefinition {
            ModuleDefinition(ModuleInfo info, IModule &module, std::shared_ptr<DynamicModule> code) : info(std::move(info)), library(code)
            {
                auto *context = SerializationContext::Get();
                context->BeginModuleCode(std::move(code));
                module.RegisterMeta();
                metadataCode = context->EndModuleCode();
            }
            ModuleInfo                   info;
            std::weak_ptr<DynamicModule> library;
            std::shared_ptr<const void>  metadataCode;
        };

        class ModuleDefinitions : public Singleton<ModuleDefinitions> {
        public:
            std::shared_ptr<DynamicModule> FindLibrary(const std::string &name) const
            {
                const auto found = definitions.find(name);
                return found == definitions.end() ? nullptr : found->second.library.lock();
            }

            const ModuleDefinition &Resolve(ModuleInfo info, IModule &module, std::shared_ptr<DynamicModule> code)
            {
                if (const auto found = definitions.find(info.name); found != definitions.end()) {
                    found->second.library = code;
                    return found->second;
                }
                const auto name              = info.name;
                const auto [entry, inserted] = definitions.try_emplace(name, std::move(info), module, std::move(code));
                SKY_ASSERT(inserted)
                return entry->second;
            }

        private:
            friend class Singleton<ModuleDefinitions>;
            ModuleDefinitions() = default;
            std::unordered_map<std::string, ModuleDefinition> definitions;
        };
    } // namespace

    ModuleManager *ModuleManager::Current()
    {
        return CurrentManager::Get()->value;
    }



    using ModuleStart = IModule *(*)(Environment *);
    using ModuleStop  = void (*)();

    void ModuleManager::RegisterModule(const ModuleInfo &info)
    {
        auto  src      = RegisterModuleImpl(info.name);
        auto &required = dependencies[info.name];
        for (const auto &dep : info.dependencies)
            if (std::find(required.begin(), required.end(), dep) == required.end())
                required.push_back(dep);
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
        staticLeases.emplace(id, std::make_shared<const std::string>(id));
        modules.emplace(id, std::move(module));
    }

    std::shared_ptr<const void> ModuleManager::RetainModule(const std::string &name) const
    {
        if (unloading)
            return {};
        if (const auto found = dynLibs.find(name); found != dynLibs.end())
            return found->second;
        const auto found = staticLeases.find(name);
        return found == staticLeases.end() ? nullptr : found->second;
    }
    bool ModuleManager::LoadModules(const StartArguments &args)
    {
        CurrentManager::Get()->value = this;
        unloading      = false;
        bool succeeded = true;
        WalkModules(
            [this, &args, &succeeded](const std::string &moduleName) {
                std::vector<std::shared_ptr<const void>> required;
                for (const auto &dependency : dependencies[moduleName]) {
                    auto lease = RetainModule(dependency);
                    if (!lease) {
                        LOG_E(TAG, "Module '%s' requires loaded '%s'", moduleName.c_str(), dependency.c_str());
                        succeeded = false;
                        modules.erase(moduleName);
                        staticLeases.erase(moduleName);
                        return;
                    }
                    required.push_back(std::move(lease));
                }
                // Statically bundled (compiled-in registration): no dlopen, just Init.
                if (auto staticIt = modules.find(moduleName); staticIt != modules.end()) {
                    LOG_I(TAG, "Load Module (static) : %s", moduleName.c_str());
                    ModuleDefinitions::Get()->Resolve({moduleName, dependencies[moduleName]}, *staticIt->second, {});
                    if (!staticIt->second->Init(args)) {
                        succeeded = false;
                        LOG_E(TAG, "Load Module (static) : %s failed", moduleName.c_str());
                        staticIt->second->Shutdown();
                        modules.erase(staticIt);
                        staticLeases.erase(moduleName);
                    }
                    return;
                }

                auto attached = std::make_shared<bool>(false);
                auto dynModule = ModuleDefinitions::Get()->FindLibrary(moduleName);
                if (!dynModule)
                    dynModule = std::shared_ptr<DynamicModule>(new DynamicModule(moduleName),
                                                               [attached, required = std::move(required)](DynamicModule *library) {
                                                                   if (*attached)
                                                                       if (auto stop = library->GetAddress<ModuleStop>("StopModule"))
                                                                           stop();
                                                                   delete library;
                                                               });
                LOG_I(TAG, "Load Module : %s", moduleName.c_str());
                if (dynModule->IsLoaded() || dynModule->Load()) {
                    auto startFn = dynModule->GetAddress<ModuleStart>("StartModule");
                    if (startFn == nullptr) {
                        succeeded = false;
                        LOG_E(TAG, "Load Module : %s failed", moduleName.c_str());
                        return;
                    }
                    dynLibs[moduleName] = dynModule;
                    auto module         = std::unique_ptr<IModule>(startFn(Environment::Get()));
                    *attached           = true;
                    if (module)
                        ModuleDefinitions::Get()->Resolve({moduleName, dependencies[moduleName]}, *module, dynModule);
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
        return succeeded;
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
        if (CurrentManager::Get()->value == this)
            CurrentManager::Get()->value = nullptr;
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

        unloading = true;
        staticLeases.clear();
        // Libraries stay owned until application registries and external callback leases release them.
    }

} // namespace sky
