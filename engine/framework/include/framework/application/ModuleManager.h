//
// Created by blues on 2023/12/28.
//

#pragma once

#include <core/std/Graph.h>
#include <core/util/DynamicModule.h>
#include <framework/interface/IModule.h>
#include <memory>
#include <optional>
#include <unordered_map>
#include <unordered_set>

namespace sky {

    struct ModuleInfo {
        std::string              name;
        std::vector<std::string> dependencies;
    };

    // Lifecycle and registration are host-thread operations; callbacks cannot mutate the executing session set.
    class ModuleManager {
    public:
        ModuleManager() = default;
        ~ModuleManager();

        using Graph             = sky::Graph;
        using vertex_descriptor = Graph::Vertex;

        bool                       RegisterModule(const ModuleInfo &info);
        static std::optional<bool> GetBuildOption(std::string_view option);

        // Register a module that is compiled into the host (static bundling); it is
        // loaded/started alongside dynamic modules but never dlopen'd. `id` is the
        // module's target name (the same name used in the module configs).
        bool AddStaticModule(const std::string &id, std::unique_ptr<IModule> module);

        bool TopoSort();

        template <typename F, typename C>
        void WalkModules(F &&func, const C &container)
        {
            for (const auto &vtx : container) {
                func(names[vtx]);
            }
        }

        template <typename F>
        void WalkModules(F &&func)
        {
            WalkModules(std::forward<F>(func), sortedContainer);
        }

        bool Tick(float time);

        bool LoadModules(const StartArguments &args);
        // Retry rejected teardown after the current Init/Start/Tick callback dispatch returns.
        bool UnLoadModules();

        bool StartModules();
        bool IsModuleInitialized(const std::string &name) const;

        template <typename Func>
        Func GetFunctionFomModule(const std::string &module, const std::string &func)
        {
            const auto found = dynLibs.find(module);
            return found == dynLibs.end() ? nullptr : found->second->GetAddress<Func>(func);
        }

    private:
        vertex_descriptor RegisterModuleImpl(const std::string &path);

        enum class Phase { Idle, Initializing, Dispatching, Stopping };
        Phase                                                     phase = Phase::Idle;
        std::unordered_map<std::string, std::vector<std::string>> dependencies;
        std::unordered_set<std::string>                           initializedModules;
        Graph                                                     dependencyGraph;
        std::vector<std::string>                                  names;
        std::vector<vertex_descriptor>                            sortedContainer;

        std::unordered_map<std::string, std::unique_ptr<DynamicModule>> dynLibs;
        std::unordered_map<std::string, std::unique_ptr<IModule>>       modules;
    };
} // namespace sky
