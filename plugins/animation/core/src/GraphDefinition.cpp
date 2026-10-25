#include <animation/graph/GraphDefinition.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <set>
#include <unordered_map>
#include <unordered_set>

namespace sky::animation {
    namespace {
        bool LessId(const Uuid &a, const Uuid &b)
        {
            return a.ToString() < b.ToString();
        }
        void Error(GraphValidation &result, const Uuid &id, std::string message, std::string pin = {})
        {
            result.diagnostics.push_back({id, std::move(pin), std::move(message)});
        }
        bool Finite(const AnimationParam &value)
        {
            return !std::holds_alternative<float>(value) || std::isfinite(std::get<float>(value));
        }
        bool Matches(GraphParameterType type, const AnimationParam &value)
        {
            return (type == GraphParameterType::Bool && std::holds_alternative<bool>(value)) ||
                   (type == GraphParameterType::Float && std::holds_alternative<float>(value));
        }
        std::vector<uint32_t> Topology(const GraphDefinition &graph)
        {
            std::unordered_map<Uuid, uint32_t> indices;
            for (uint32_t i = 0; i < graph.nodes.size(); ++i) {
                indices.emplace(graph.nodes[i].id, i);
            }
            std::vector<uint32_t>              degrees(graph.nodes.size(), 0);
            std::vector<std::vector<uint32_t>> parents(graph.nodes.size());
            for (const auto &link : graph.links) {
                const auto from = indices.find(link.from), to = indices.find(link.to);
                if (from != indices.end() && to != indices.end()) {
                    ++degrees[to->second];
                    parents[from->second].push_back(to->second);
                }
            }
            std::vector<uint32_t> ordered;
            for (uint32_t i = 0; i < degrees.size(); ++i) {
                if (!degrees[i]) {
                    ordered.push_back(i);
                }
            }
            for (size_t i = 0; i < ordered.size(); ++i) {
                for (const auto parent : parents[ordered[i]]) {
                    if (--degrees[parent] == 0) {
                        ordered.push_back(parent);
                    }
                }
            }
            return ordered;
        }
    } // namespace

    GraphValidation ValidateGraph(const GraphDefinition &graph, GraphValidationMode mode)
    {
        GraphValidation result;
        if (graph.version != GRAPH_VERSION) {
            Error(result, graph.document, "Unsupported graph version; migrate the source and recook");
        }
        if (!graph.document) {
            Error(result, {}, "Graph document ID is required");
        }
        if (graph.nodes.size() > 4096 || graph.links.size() > 8192 || graph.parameters.size() > 256 || graph.states.size() > 256 ||
            graph.transitions.size() > 4096) {
            Error(result, graph.document, "Graph exceeds the supported node/link/parameter/state/transition limits");
            return result;
        }
        std::unordered_set<Uuid>           identities;
        std::unordered_set<std::string>    parameterNames, stateNames;
        std::unordered_map<Uuid, uint32_t> nodes, states, parameters;
        const auto                         identity = [&](const Uuid &id) {
            if (!id || !identities.emplace(id).second) {
                Error(result, id, "Object ID is empty or duplicated");
            }
        };
        for (uint32_t i = 0; i < graph.parameters.size(); ++i) {
            const auto &parameter = graph.parameters[i];
            identity(parameter.id);
            parameters.emplace(parameter.id, i);
            if (parameter.name.empty() || parameter.name.size() > 256 || !parameterNames.emplace(parameter.name).second) {
                Error(result, parameter.id, "Parameter name is empty, too long, or duplicated");
            }
            if (!Matches(parameter.type, parameter.value) || !Finite(parameter.value)) {
                Error(result, parameter.id, "Parameter default has the wrong type or is nonfinite");
            }
        }
        for (uint32_t i = 0; i < graph.nodes.size(); ++i) {
            const auto &node = graph.nodes[i];
            identity(node.id);
            nodes.emplace(node.id, i);
            if (node.kind != GraphNodeKind::Clip && node.kind != GraphNodeKind::Blend2 && node.kind != GraphNodeKind::Output) {
                Error(result, node.id, "Unknown node kind; nested state machines are unsupported");
            }
            if (!std::isfinite(node.rate) || node.rate < 0.f || !std::isfinite(node.alpha)) {
                Error(result, node.id, "Node rate/alpha is invalid");
            }
        }
        for (uint32_t i = 0; i < graph.states.size(); ++i) {
            const auto &state = graph.states[i];
            identity(state.id);
            states.emplace(state.id, i);
            if (state.name.empty() || state.name.size() > 256 || !stateNames.emplace(state.name).second) {
                Error(result, state.id, "State name is empty, too long, or duplicated");
            }
        }
        for (const auto &transition : graph.transitions) {
            identity(transition.id);
            if (!std::isfinite(transition.duration) || transition.duration < 0.f || !std::isfinite(transition.exitTime) ||
                transition.exitTime < 0.f || transition.exitTime > 1.f) {
                Error(result, transition.id, "Transition duration or normalized exit time is invalid");
            }
            if (transition.conditions.size() > 256) {
                Error(result, transition.id, "Too many transition conditions");
                continue;
            }
            for (const auto &condition : transition.conditions) {
                if (!Finite(condition.value) || condition.comparison > GraphComparison::GreaterEqual) {
                    Error(result, transition.id, "Invalid comparison or condition value");
                }
            }
        }
        for (const auto &link : graph.links) {
            if (link.output != GraphPin::Pose || (link.input != GraphPin::InputA && link.input != GraphPin::InputB)) {
                Error(result, link.to, "Link pins have incompatible types", "input");
            }
        }
        if (!result || mode == GraphValidationMode::Draft) {
            return result;
        }
        if (!graph.skeleton) {
            Error(result, graph.document, "Graph requires a skeleton");
        }
        std::vector<std::array<uint32_t, 2>> inputs(graph.nodes.size(), {UINT32_MAX, UINT32_MAX});
        std::vector<uint32_t>                parentCounts(graph.nodes.size(), 0);
        for (const auto &link : graph.links) {
            const auto from = nodes.find(link.from), to = nodes.find(link.to);
            if (from == nodes.end() || to == nodes.end()) {
                Error(result, link.to, "Link references an unknown node", "input");
                continue;
            }
            const auto pin = link.input == GraphPin::InputA ? 0u : 1u;
            if (inputs[to->second][pin] != UINT32_MAX) {
                Error(result, link.to, "Input pin is connected more than once", pin == 0 ? "A" : "B");
            }
            inputs[to->second][pin] = from->second;
            ++parentCounts[from->second];
            if (graph.nodes[from->second].kind == GraphNodeKind::Output) {
                Error(result, link.from, "Output cannot feed another node");
            }
        }
        for (uint32_t i = 0; i < graph.nodes.size(); ++i) {
            const auto &node = graph.nodes[i];
            if (parentCounts[i] > 1) {
                Error(result, node.id, "A pose subtree cannot have multiple consumers");
            }
            const bool a = inputs[i][0] != UINT32_MAX, b = inputs[i][1] != UINT32_MAX;
            if ((node.kind == GraphNodeKind::Clip && (a || b)) || (node.kind == GraphNodeKind::Blend2 && (!a || !b)) ||
                (node.kind == GraphNodeKind::Output && (!a || b))) {
                Error(result, node.id, "Pose inputs do not match this node kind", "input");
            }
            if (node.kind == GraphNodeKind::Clip && !node.clip) {
                Error(result, node.id, "Clip asset is missing", "clip");
            }
            if (node.alphaParameter) {
                const auto parameter = parameters.find(node.alphaParameter);
                if (node.kind != GraphNodeKind::Blend2 || parameter == parameters.end() ||
                    graph.parameters[parameter->second].type != GraphParameterType::Float) {
                    Error(result, node.id, "Blend alpha requires a float parameter", "alpha");
                }
            }
        }
        if (Topology(graph).size() != graph.nodes.size()) {
            Error(result, graph.document, "Pose graph contains a cycle");
        }
        std::vector<Uuid> roots;
        if (graph.states.empty()) {
            roots.push_back(graph.output);
            if (graph.entryState || !graph.transitions.empty()) {
                Error(result, graph.document, "Pose graphs cannot contain state-machine control data");
            }
        } else {
            if (graph.output || !states.contains(graph.entryState)) {
                Error(result, graph.document, "State graph requires one entry state and no global pose output");
            }
            for (const auto &state : graph.states) {
                roots.push_back(state.output);
            }
        }
        std::vector<int32_t> owners(graph.nodes.size(), -1);
        for (uint32_t owner = 0; owner < roots.size(); ++owner) {
            const auto root = nodes.find(roots[owner]);
            if (root == nodes.end() || graph.nodes[root->second].kind != GraphNodeKind::Output) {
                Error(result, roots[owner], "Root must reference an Output node");
                continue;
            }
            std::vector<uint32_t> pending{root->second};
            while (!pending.empty()) {
                const auto index = pending.back();
                pending.pop_back();
                if (owners[index] != -1) {
                    if (owners[index] != static_cast<int32_t>(owner)) {
                        Error(result, graph.nodes[index].id, "States must own disjoint pose trees");
                    }
                    continue;
                }
                owners[index] = static_cast<int32_t>(owner);
                for (const auto child : inputs[index]) {
                    if (child != UINT32_MAX) {
                        pending.push_back(child);
                    }
                }
            }
        }
        for (uint32_t i = 0; i < owners.size(); ++i) {
            if (owners[i] == -1) {
                Error(result, graph.nodes[i].id, "Node is not reachable from a graph/state output");
            }
        }
        for (const auto &transition : graph.transitions) {
            const auto from = states.find(transition.from), to = states.find(transition.to);
            if (from == states.end() || to == states.end() || from == to) {
                Error(result, transition.id, "Transition has unknown endpoints or is a self-transition");
            }
            if (transition.hasExitTime) {
                const auto clock = nodes.find(transition.timeSourceNode);
                if (clock == nodes.end() || graph.nodes[clock->second].kind != GraphNodeKind::Clip || from == states.end() ||
                    owners[clock->second] != static_cast<int32_t>(from->second)) {
                    Error(result, transition.id, "Exit time requires an explicit Clip clock in the source state", "timeSourceNode");
                }
            }
            for (const auto &condition : transition.conditions) {
                const auto parameter = parameters.find(condition.parameter);
                if (parameter == parameters.end() || !Matches(graph.parameters[parameter->second].type, condition.value)) {
                    Error(result, transition.id, "Condition parameter is missing or has the wrong type");
                    continue;
                }
                if (graph.parameters[parameter->second].type == GraphParameterType::Bool && condition.comparison != GraphComparison::Equal &&
                    condition.comparison != GraphComparison::NotEqual) {
                    Error(result, transition.id, "Boolean conditions only support equality and inequality");
                }
            }
        }
        return result;
    }

    GraphValidation ValidateGraphSource(const GraphSource &source, GraphValidationMode mode)
    {
        auto        result = ValidateGraph(source.definition, mode);
        const auto &editor = source.editor;
        if (!std::isfinite(editor.panX) || !std::isfinite(editor.panY) || !std::isfinite(editor.zoom) || editor.zoom <= 0.f ||
            editor.nodes.size() > 4096) {
            Error(result, source.definition.document, "Invalid editor view layout");
            return result;
        }
        std::unordered_set<Uuid> ids;
        for (const auto &node : editor.nodes) {
            if (!node.node || !ids.emplace(node.node).second || !std::isfinite(node.x) || !std::isfinite(node.y)) {
                Error(result, node.node, "Invalid or duplicated editor node layout");
            }
        }
        return result;
    }

    GraphValidation CompileGraphDefinition(const GraphDefinition &graph, CookedGraph &out)
    {
        auto result = ValidateGraph(graph);
        if (!result) {
            return result;
        }
        CookedGraph next;
        next.definition        = graph;
        const auto sortObjects = [](auto &objects) {
            std::sort(objects.begin(), objects.end(), [](const auto &a, const auto &b) { return LessId(a.id, b.id); });
        };
        sortObjects(next.definition.nodes);
        sortObjects(next.definition.parameters);
        sortObjects(next.definition.states);
        std::sort(next.definition.transitions.begin(), next.definition.transitions.end(),
                  [](const auto &a, const auto &b) { return a.priority != b.priority ? a.priority > b.priority : LessId(a.id, b.id); });
        std::sort(next.definition.links.begin(), next.definition.links.end(),
                  [](const auto &a, const auto &b) { return a.to != b.to ? LessId(a.to, b.to) : a.input < b.input; });
        for (auto &node : next.definition.nodes) {
            node.alpha = std::clamp(node.alpha, 0.f, 1.f);
        }
        next.topologicalNodes = Topology(next.definition);
        next.dependencies.push_back(graph.skeleton);
        for (const auto &node : graph.nodes) {
            if (node.kind == GraphNodeKind::Clip) {
                next.dependencies.push_back(node.clip);
            }
        }
        std::sort(next.dependencies.begin(), next.dependencies.end(), LessId);
        next.dependencies.erase(std::unique(next.dependencies.begin(), next.dependencies.end()), next.dependencies.end());
        out = std::move(next);
        return result;
    }
} // namespace sky::animation
