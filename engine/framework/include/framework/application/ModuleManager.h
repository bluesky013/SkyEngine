//
// Created by blues on 2023/12/28.
//

#pragma once

#include <core/util/DynamicModule.h>
#include <core/std/Graph.h>
#include <framework/interface/IModule.h>
#include <unordered_map>

namespace sky {

    struct ModuleInfo {
        std::string name;
        std::vector<std::string> dependencies;
    };

    class ModuleManager {
    public:
        ModuleManager() = default;
        ~ModuleManager() = default;

        using Graph = sky::Graph;
        using vertex_descriptor = Graph::Vertex;

        void RegisterModule(const ModuleInfo &info);

        // Register a module that is compiled into the host (static bundling); it is
        // loaded/started alongside dynamic modules but never dlopen'd. `id` is the
        // module's target name (the same name used in the module configs).
        void AddStaticModule(const std::string &id, std::unique_ptr<IModule> module);

        void TopoSort();

        template <typename F, typename C>
        void WalkModules(F&& func, const C &container)
        {
            for (const auto &vtx : container) {
                func(names[vtx]);
            }
        }

        template <typename F>
        void WalkModules(F&& func)
        {
            WalkModules(std::forward<F>(func), sortedContainer);
        }

        void Tick(float time);

        bool LoadModules(const StartArguments &args);
        void UnLoadModules();

        // Manager currently running LoadModules; lets statically linked modules
        // acquire real leases when no ISystemNotify host is registered.
        static ModuleManager *Current();

        void                        StartModules();
        std::shared_ptr<const void> RetainModule(const std::string &name) const;

        template <typename Func>
        Func GetFunctionFomModule(const std::string &module, const std::string &func)
        {
            return dynLibs[module]->GetAddress<Func>(func);
        }
    private:
        vertex_descriptor RegisterModuleImpl(const std::string &path);

        bool                                                         unloading = false;
        std::unordered_map<std::string, std::vector<std::string>>    dependencies;
        std::unordered_map<std::string, std::shared_ptr<const void>> staticLeases;
        Graph                                                        dependencyGraph;
        std::vector<std::string>                                     names;
        std::vector<vertex_descriptor>                               sortedContainer;

        std::unordered_map<std::string, std::shared_ptr<DynamicModule>> dynLibs;
        std::unordered_map<std::string, std::unique_ptr<IModule>>       modules;
    };
} // namespace sky
