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

        void LoadModules(const StartArguments &args);
        void UnLoadModules();

        void StartModules();

        template <typename Func>
        Func GetFunctionFomModule(const std::string &module, const std::string &func)
        {
            return dynLibs[module]->GetAddress<Func>(func);
        }
    private:
        vertex_descriptor RegisterModuleImpl(const std::string &path);

        Graph dependencyGraph;
        std::vector<std::string> names;
        std::vector<vertex_descriptor> sortedContainer;

        std::unordered_map<std::string, std::unique_ptr<DynamicModule>> dynLibs;
        std::unordered_map<std::string, std::unique_ptr<IModule>> modules;
    };
} // namespace sky
