#include "prepare/SerialPrepareExecutor.h"

#include <algorithm>
#include <set>

namespace sky::aurora {

    bool PrepareResult::Succeeded() const
    {
        return !cancelled && error.empty() &&
               std::all_of(nodes.begin(), nodes.end(), [](const auto &node) { return node.state == PrepareNodeState::SUCCEEDED; });
    }

    PrepareResult ExecuteSerialPrepareChecked(const DependencyPlan &plan, const std::function<PrepareNodeResult(SystemId)> &kernel)
    {
        SerialPrepareResult result;
        std::set<SystemId>  ids;
        for (const auto &system : plan.systems) {
            if (system.id == 0 || !ids.insert(system.id).second) {
                result.error = "Invalid prepare system identity";
                return result;
            }
        }
        std::vector<std::vector<uint32_t>> dependencies(plan.systems.size());
        for (const auto &edge : plan.edges) {
            if (edge.from >= edge.to || edge.to >= plan.systems.size()) {
                result.error = "Prepare plan is not in dependency order";
                return result;
            }
            dependencies[edge.to].push_back(edge.from);
        }
        if (!plan.systems.empty() && !kernel) {
            result.error = "Missing prepare kernel";
            return result;
        }
        result.nodes.reserve(plan.systems.size());
        for (size_t i = 0; i < plan.systems.size(); ++i) {
            PrepareNodeResult node{plan.systems[i].id};
            const bool        ready = std::all_of(dependencies[i].begin(), dependencies[i].end(),
                                                  [&](uint32_t index) { return result.nodes[index].state == PrepareNodeState::SUCCEEDED; });
            if (ready) {
                node = kernel(node.system);
            } else {
                node.diagnostic = "Prepare dependency did not succeed";
            }
            result.nodes.push_back(std::move(node));
        }
        return result;
    }

    SerialPrepareResult ExecuteSerialPrepare(const DependencyPlan &plan, const std::function<void(SystemId)> &kernel)
    {
        if (!plan.systems.empty() && !kernel) {
            return {{}, "Missing prepare kernel"};
        }
        return ExecuteSerialPrepareChecked(plan, [&](SystemId id) {
            kernel(id);
            return PrepareNodeResult{id, PrepareNodeState::SUCCEEDED};
        });
    }

} // namespace sky::aurora
