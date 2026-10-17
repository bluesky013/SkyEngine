#include <core/ecs/Dependency.h>

#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <tuple>

namespace sky {

    DependencyCompileResult CompileDependencyPlan(std::span<const SystemDesc> input)
    {
        if (input.size() > std::numeric_limits<uint32_t>::max()) {
            return DependencyDiagnostic{DependencyErrorCode::INVALID_SYSTEM, "Too many systems"};
        }
        std::vector<SystemDesc> systems(input.begin(), input.end());
        std::sort(systems.begin(), systems.end(),
                  [](const auto &a, const auto &b) { return std::tie(a.phase, a.order, a.id) < std::tie(b.phase, b.order, b.id); });
        const auto                                             count = static_cast<uint32_t>(systems.size());
        std::map<SystemId, uint32_t>                           indices;
        std::map<std::pair<AccessKind, uint64_t>, std::string> tags;
        for (uint32_t i = 0; i < count; ++i) {
            auto &system = systems[i];
            if (system.id == 0 || system.name.empty()) {
                return DependencyDiagnostic{DependencyErrorCode::INVALID_SYSTEM, "System requires a nonzero id and a name"};
            }
            if (!indices.emplace(system.id, i).second) {
                return DependencyDiagnostic{DependencyErrorCode::DUPLICATE_SYSTEM, "Duplicate system id: " + std::to_string(system.id)};
            }
            std::map<AccessKey, AccessDesc> normalized;
            for (const auto &access : system.accesses) {
                const auto &key = access.key;
                if (key.domain.empty() || key.owner.instance == 0 || key.owner.generation == 0 || key.identity == 0 || access.tag.empty() ||
                    (key.kind != AccessKind::COMPONENT && key.kind != AccessKind::RESOURCE) ||
                    (access.mode != AccessMode::READ && access.mode != AccessMode::WRITE)) {
                    return DependencyDiagnostic{DependencyErrorCode::INVALID_ACCESS, "Invalid access in " + system.name, {}, key};
                }
                const auto [tagIt, inserted] = tags.emplace(std::make_pair(key.kind, key.identity), access.tag);
                if (!inserted && tagIt->second != access.tag) {
                    return DependencyDiagnostic{DependencyErrorCode::TAG_COLLISION,
                                                "Access identity collision in " + system.name + ": " + tagIt->second + " / " + access.tag,
                                                {},
                                                key};
                }
                auto [it, added] = normalized.emplace(key, access);
                if (!added && access.mode == AccessMode::WRITE) {
                    it->second.mode = AccessMode::WRITE;
                }
            }
            system.accesses.clear();
            for (auto &[key, access] : normalized) {
                system.accesses.push_back(std::move(access));
            }
            for (auto *constraints : {&system.before, &system.after}) {
                std::sort(constraints->begin(), constraints->end());
                constraints->erase(std::unique(constraints->begin(), constraints->end()), constraints->end());
            }
        }

        std::map<std::pair<uint32_t, uint32_t>, std::set<DependencyOrigin>> semanticEdges;
        std::vector<std::vector<uint32_t>>                                  successors(count), predecessors(count);
        std::vector<uint32_t>                                               indegree(count, 0);
        const auto addSemanticEdge = [&](uint32_t from, uint32_t to, DependencyOrigin origin) {
            auto [it, inserted] = semanticEdges.try_emplace({from, to});
            it->second.insert(origin);
            if (inserted) {
                successors[from].push_back(to);
                predecessors[to].push_back(from);
                ++indegree[to];
            }
        };
        uint32_t previousBegin = 0;
        uint32_t begin         = 0;
        while (begin < count) {
            uint32_t end = begin + 1;
            while (end < count && systems[end].phase == systems[begin].phase) {
                ++end;
            }
            for (uint32_t from = previousBegin; from < begin; ++from) {
                for (uint32_t to = begin; to < end; ++to) {
                    addSemanticEdge(from, to, DependencyOrigin::PHASE);
                }
            }
            previousBegin = begin;
            begin         = end;
        }
        for (uint32_t i = 0; i < count; ++i) {
            for (const bool before : {true, false}) {
                for (SystemId id : before ? systems[i].before : systems[i].after) {
                    const auto                origin = before ? DependencyOrigin::BEFORE : DependencyOrigin::AFTER;
                    const DependencyIssueEdge issue{before ? systems[i].id : id, before ? id : systems[i].id, origin};
                    const auto                target = indices.find(id);
                    if (target == indices.end()) {
                        return DependencyDiagnostic{DependencyErrorCode::UNKNOWN_SYSTEM,
                                                    "Unknown system " + std::to_string(id) + " referenced by " + systems[i].name,
                                                    {issue}};
                    }
                    const uint32_t from = before ? i : target->second;
                    const uint32_t to   = before ? target->second : i;
                    if (systems[from].phase > systems[to].phase) {
                        return DependencyDiagnostic{DependencyErrorCode::PHASE_CONFLICT,
                                                    "Reverse phase constraint: " + systems[from].name + " -> " + systems[to].name,
                                                    {issue}};
                    }
                    addSemanticEdge(from, to, origin);
                }
            }
        }

        std::set<uint32_t> ready;
        for (uint32_t i = 0; i < count; ++i) {
            if (indegree[i] == 0) {
                ready.insert(i);
            }
        }
        std::vector<uint32_t> order;
        while (!ready.empty()) {
            const auto node = *ready.begin();
            ready.erase(ready.begin());
            order.push_back(node);
            for (uint32_t next : successors[node]) {
                if (--indegree[next] == 0) {
                    ready.insert(next);
                }
            }
        }
        if (order.size() != count) {
            uint32_t node = 0;
            while (indegree[node] == 0) {
                ++node;
            }
            std::vector<uint32_t>      walk;
            std::map<uint32_t, size_t> visited;
            while (!visited.contains(node)) {
                visited.emplace(node, walk.size());
                walk.push_back(node);
                auto &parents = predecessors[node];
                std::sort(parents.begin(), parents.end());
                node = *std::find_if(parents.begin(), parents.end(), [&](uint32_t parent) { return indegree[parent] != 0; });
            }
            DependencyDiagnostic error{DependencyErrorCode::CYCLE, "Dependency cycle:"};
            const size_t         first = visited.at(node);
            for (size_t i = first; i < walk.size(); ++i) {
                const uint32_t from = walk[i + 1 < walk.size() ? i + 1 : first];
                const uint32_t to   = walk[i];
                error.message += " " + systems[from].name + " -> " + systems[to].name + ";";
                for (auto origin : semanticEdges.at({from, to})) {
                    error.edges.push_back({systems[from].id, systems[to].id, origin});
                }
            }
            return error;
        }

        DependencyPlan        plan;
        std::vector<uint32_t> remap(count);
        for (uint32_t i = 0; i < count; ++i) {
            remap[order[i]] = i;
            plan.systems.push_back(std::move(systems[order[i]]));
        }
        for (const auto &[pair, origins] : semanticEdges) {
            for (auto origin : origins) {
                plan.edges.push_back({remap[pair.first], remap[pair.second], origin, std::nullopt});
            }
        }
        struct HazardState {
            std::optional<uint32_t> writer;
            std::vector<uint32_t>   readers;
        };
        std::map<AccessKey, HazardState> hazards;
        for (uint32_t node = 0; node < count; ++node) {
            for (const auto &access : plan.systems[node].accesses) {
                auto &state = hazards[access.key];
                if (state.writer) {
                    plan.edges.push_back(
                        {*state.writer, node, access.mode == AccessMode::READ ? DependencyOrigin::RAW : DependencyOrigin::WAW, access.key});
                }
                if (access.mode == AccessMode::READ) {
                    state.readers.push_back(node);
                } else {
                    for (uint32_t reader : state.readers) {
                        plan.edges.push_back({reader, node, DependencyOrigin::WAR, access.key});
                    }
                    state.readers.clear();
                    state.writer = node;
                }
            }
        }
        std::sort(plan.edges.begin(), plan.edges.end());
        return plan;
    }

} // namespace sky
