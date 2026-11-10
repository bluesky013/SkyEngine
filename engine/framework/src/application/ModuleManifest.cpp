//
// Created on 2026/11/10.
//

#include <framework/application/ModuleManifest.h>

#include <core/std/Graph.h>
#include <rapidjson/document.h>
#include <unordered_map>
#include <unordered_set>

namespace sky {
    ModuleManifestResult ParseModuleManifest(std::string_view json, ModuleBuildOptionLookup options)
    {
        if (json.empty() || json.size() > 1024 * 1024 || !options)
            return std::string("Module manifest is empty, oversized or has no build-option lookup");
        rapidjson::Document document;
        document.Parse<rapidjson::kParseIterativeFlag>(json.data(), json.size());
        if (document.HasParseError() || !document.IsObject() || !document.HasMember("modules") || !document["modules"].IsArray())
            return std::string("Module manifest requires an object with a modules array");
        const auto validName = [](const rapidjson::Value &value) {
            if (!value.IsString() || !value.GetStringLength() || value.GetStringLength() > 128)
                return false;
            const std::string_view name(value.GetString(), value.GetStringLength());
            return name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.-") == std::string_view::npos;
        };
        const auto entries = document["modules"].GetArray();
        if (entries.Size() > 1024)
            return std::string("Module manifest exceeds the module limit");
        std::vector<ModuleInfo>                                   parsed;
        std::unordered_set<std::string>                           names, disabled;
        std::unordered_map<std::string, std::vector<std::string>> optionalDependencies;
        size_t                                                    dependencyCount = 0;
        for (const auto &entry : entries) {
            if (!entry.IsObject() || !entry.HasMember("name") || !validName(entry["name"]))
                return std::string("Module name must be a nonempty identifier");
            ModuleInfo info{entry["name"].GetString(), {}};
            if (!names.insert(info.name).second)
                return "Duplicate module declaration: " + info.name;
            bool enabled = true;
            if (entry.HasMember("buildOption")) {
                if (!validName(entry["buildOption"]))
                    return "Invalid buildOption for module " + info.name;
                const auto option = options(entry["buildOption"].GetString());
                if (!option)
                    return "Unknown buildOption for module " + info.name + ": " + entry["buildOption"].GetString();
                enabled = *option;
            }
            std::unordered_set<std::string> unique;
            for (const char *field : {"dependencies", "optionalDependencies"}) {
                if (!entry.HasMember(field))
                    continue;
                if (!entry[field].IsArray())
                    return std::string(field) + " must be an array for module " + info.name;
                for (const auto &dependency : entry[field].GetArray()) {
                    if (!validName(dependency) || ++dependencyCount > 4096)
                        return "Invalid or excessive dependencies for module " + info.name;
                    if (!unique.insert(dependency.GetString()).second)
                        return "Duplicate dependency for module " + info.name;
                    if (std::string_view(field) == "dependencies")
                        info.dependencies.emplace_back(dependency.GetString());
                    else
                        optionalDependencies[info.name].emplace_back(dependency.GetString());
                }
            }
            if (enabled)
                parsed.push_back(std::move(info));
            else
                disabled.insert(info.name);
        }
        for (auto &info : parsed) {
            for (const auto &dependency : optionalDependencies[info.name]) {
                if (!names.contains(dependency))
                    return "Unknown optional dependency for module " + info.name + ": " + dependency;
                if (!disabled.contains(dependency))
                    info.dependencies.push_back(dependency);
            }
        }
        Graph                                          graph;
        std::unordered_map<std::string, Graph::Vertex> vertices;
        const auto                                     vertex = [&](const std::string &name) {
            const auto found = vertices.find(name);
            if (found != vertices.end())
                return found->second;
            const auto value = graph.AddVertex();
            vertices.emplace(name, value);
            return value;
        };
        for (const auto &info : parsed) {
            const auto source = vertex(info.name);
            for (const auto &dependency : info.dependencies) {
                if (disabled.contains(dependency))
                    return "Module " + info.name + " requires disabled module " + dependency;
                graph.AddEdge(source, vertex(dependency));
            }
        }
        graph.TopologicalSort();
        if (graph.IsCyclic())
            return std::string("Module manifest contains a dependency cycle");
        return parsed;
    }
} // namespace sky
