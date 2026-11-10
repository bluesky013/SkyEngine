#include "animation/editor/GraphDocument.h"

#include <algorithm>
#include <map>
#include <set>

namespace sky::animation::editor {
    namespace {
        GraphValidation Invalid(const Uuid &id, std::string message)
        {
            return {{{id, {}, std::move(message)}}};
        }
    } // namespace

    GraphDocumentStorage GraphSourceStorage(const Uuid &asset)
    {
        return sky::editor::MakeAssetSourceStorage(asset, 8u * 1024u * 1024u);
    }

    class GraphDocument::Edit final : public sky::editor::UndoCommand {
    public:
        Edit(GraphDocument &document, std::string label, GraphSource next)
            : UndoCommand(std::move(label)), document(document), before(document.source), after(std::move(next))
        {
        }
        void Do() override
        {
            document.SetSource(after);
        }
        void Undo() override
        {
            document.SetSource(before);
        }

    private:
        GraphDocument &document;
        GraphSource    before, after;
    };

    GraphDocument::GraphDocument(Uuid asset, GraphDocumentStorage storage) : Document(asset.ToString()), asset(asset), storage(std::move(storage))
    {
        source.definition.document = asset;
        Refresh();
        ClearDirty();
    }
    GraphDocument::GraphDocument(Uuid asset) : GraphDocument(asset, GraphSourceStorage(asset))
    {
    }
    GraphDocument::~GraphDocument()
    {
        commands.Clear();
    }
    void GraphDocument::Refresh()
    {
        diagnostics = ValidateGraph(source.definition);
        std::string encoded;
        const auto  valid = WriteGraphSource(source, encoded);
        state.Changed(*this, encoded, static_cast<bool>(valid));
    }
    void GraphDocument::SetSource(const GraphSource &value)
    {
        source = value;
        lastError.clear();
        Refresh();
    }
    bool GraphDocument::Load()
    {
        std::string text;
        const auto  status = storage.read ? storage.read(text) : FileOperationStatus{FileOperationError::Unsupported, "Graph reader unavailable"};
        if (!status) {
            lastError = status.message;
            return false;
        }
        GraphSource next;
        const auto  validation = ParseGraphSource(text, next, asset);
        if (!validation) {
            lastError = validation.diagnostics.front().message;
            return false;
        }
        const auto encoded = WriteGraphSource(next, text);
        if (!encoded) {
            lastError = encoded.diagnostics.front().message;
            return false;
        }
        source = std::move(next);
        lastError.clear();
        diagnostics = ValidateGraph(source.definition);
        state.Loaded(*this, commands, std::move(text));
        return true;
    }
    bool GraphDocument::Save()
    {
        if (!state.BeginSave(lastError)) {
            return false;
        }
        std::string text;
        const auto  validation = WriteGraphSource(source, text);
        if (!validation) {
            lastError = validation.diagnostics.front().message;
            return false;
        }
        const auto status = storage.write ? storage.write(text) : FileOperationStatus{FileOperationError::Unsupported, "Graph writer unavailable"};
        if (!status) {
            lastError = status.message;
            return false;
        }
        lastError.clear();
        diagnostics = ValidateGraph(source.definition);
        state.Saved(*this, std::move(text));
        if (storage.requestCook)
            storage.requestCook(asset);
        return true;
    }
    bool GraphDocument::FlushPendingSave()
    {
        if (!state.IsSavePending())
            return true;
        return Save();
    }
    GraphValidation GraphDocument::Apply(std::string label, GraphSource next)
    {
        if (next.definition.document != source.definition.document) {
            lastError = "Document identity cannot change";
            return Invalid(asset, lastError);
        }
        std::string encoded, current;
        auto        valid = WriteGraphSource(next, encoded);
        if (!valid) {
            lastError = valid.diagnostics.front().message;
            return valid;
        }
        const auto currentStatus = WriteGraphSource(source, current);
        if (!currentStatus) {
            lastError = currentStatus.diagnostics.front().message;
            return currentStatus;
        }
        lastError.clear();
        if (current != encoded)
            commands.Execute(std::make_unique<Edit>(*this, std::move(label), std::move(next)));
        return {};
    }
    sky::editor::UndoCommandPtr GraphDocument::MakeEdit(std::string label, GraphSource next)
    {
        if (next.definition.document != source.definition.document) {
            lastError = "Document identity cannot change";
            return nullptr;
        }
        std::string encoded, current;
        const auto  validation = WriteGraphSource(next, encoded);
        if (!validation) {
            lastError = validation.diagnostics.front().message;
            return nullptr;
        }
        const auto currentStatus = WriteGraphSource(source, current);
        if (!currentStatus) {
            lastError = currentStatus.diagnostics.front().message;
            return {};
        }
        lastError.clear();
        return encoded == current ? nullptr : std::make_unique<Edit>(*this, std::move(label), std::move(next));
    }
    Uuid GraphDocument::AddNode(GraphNodeKind kind, float x, float y)
    {
        auto       next = source;
        const auto id   = Uuid::Create();
        next.definition.nodes.push_back({.id = id, .kind = kind});
        next.editor.nodes.push_back({id, x, y});
        if (kind == GraphNodeKind::Output && !next.definition.output && next.definition.states.empty())
            next.definition.output = id;
        return Apply("Add node", std::move(next)) ? id : Uuid{};
    }
    Uuid GraphDocument::AddParameter(std::string name, GraphParameterType type)
    {
        auto       next = source;
        const auto id   = Uuid::Create();
        next.definition.parameters.push_back(
            {id, std::move(name), type, type == GraphParameterType::Bool ? AnimationParam(false) : AnimationParam(0.f)});
        return Apply("Add parameter", std::move(next)) ? id : Uuid{};
    }
    Uuid GraphDocument::AddState(std::string name, const Uuid &output)
    {
        auto       next = source;
        const auto id   = Uuid::Create();
        next.definition.states.push_back({id, std::move(name), output});
        next.definition.output = {};
        if (!next.definition.entryState)
            next.definition.entryState = id;
        return Apply("Add state", std::move(next)) ? id : Uuid{};
    }
    Uuid GraphDocument::AddTransition(const Uuid &from, const Uuid &to)
    {
        const auto exists = [&](const Uuid &id) {
            return std::ranges::find(source.definition.states, id, &GraphState::id) != source.definition.states.end();
        };
        if (!exists(from) || !exists(to) || from == to) {
            lastError = "Transition requires two distinct existing states";
            return {};
        }
        auto       next = source;
        const auto id   = Uuid::Create();
        next.definition.transitions.push_back({.id = id, .from = from, .to = to});
        return Apply("Add transition", std::move(next)) ? id : Uuid{};
    }
    GraphValidation GraphDocument::Connect(const Uuid &from, const Uuid &to, GraphPin input)
    {
        const auto outputNode = std::ranges::find(source.definition.nodes, from, &GraphNode::id);
        const auto inputNode  = std::ranges::find(source.definition.nodes, to, &GraphNode::id);
        if (from == to || outputNode == source.definition.nodes.end() || inputNode == source.definition.nodes.end() ||
            outputNode->kind == GraphNodeKind::Output || inputNode->kind == GraphNodeKind::Clip ||
            (input != GraphPin::InputA && input != GraphPin::InputB) || (input == GraphPin::InputB && inputNode->kind != GraphNodeKind::Blend2))
            return Invalid(to, "Incompatible pose connection");
        auto next = source;
        std::erase_if(next.definition.links, [&](const auto &link) { return link.to == to && link.input == input; });
        next.definition.links.push_back({from, GraphPin::Pose, to, input});
        return Apply("Connect pose", std::move(next));
    }
    GraphValidation GraphDocument::Disconnect(const Uuid &to, GraphPin input)
    {
        auto next = source;
        std::erase_if(next.definition.links, [&](const auto &link) { return link.to == to && link.input == input; });
        return Apply("Disconnect pose", std::move(next));
    }
    GraphValidation GraphDocument::Remove(std::span<const Uuid> ids)
    {
        const std::set<Uuid> removed(ids.begin(), ids.end());
        auto                 next  = source;
        auto                &graph = next.definition;
        const auto eraseObjects    = [&](auto &values) { std::erase_if(values, [&](const auto &value) { return removed.contains(value.id); }); };
        eraseObjects(graph.nodes);
        eraseObjects(graph.states);
        eraseObjects(graph.parameters);
        eraseObjects(graph.transitions);
        std::erase_if(graph.links, [&](const auto &link) { return removed.contains(link.from) || removed.contains(link.to); });
        std::erase_if(graph.transitions, [&](const auto &value) { return removed.contains(value.from) || removed.contains(value.to); });
        std::erase_if(next.editor.nodes, [&](const auto &value) { return removed.contains(value.node); });
        for (auto &node : graph.nodes)
            if (removed.contains(node.alphaParameter))
                node.alphaParameter = {};
        for (auto &state : graph.states)
            if (removed.contains(state.output))
                state.output = {};
        for (auto &transition : graph.transitions) {
            std::erase_if(transition.conditions, [&](const auto &condition) { return removed.contains(condition.parameter); });
            if (removed.contains(transition.timeSourceNode)) {
                transition.timeSourceNode = {};
                transition.hasExitTime    = false;
            }
        }
        if (removed.contains(graph.output))
            graph.output = {};
        if (removed.contains(graph.entryState))
            graph.entryState = {};
        return Apply("Delete graph objects", std::move(next));
    }
    GraphValidation GraphDocument::MoveNodes(std::span<const GraphNodePosition> positions)
    {
        auto next = source;
        for (const auto &position : positions) {
            if (std::ranges::find(next.definition.nodes, position.node, &GraphNode::id) == next.definition.nodes.end())
                return Invalid(position.node, "Cannot move an unknown node");
            const auto found = std::ranges::find(next.editor.nodes, position.node, &GraphNodePosition::node);
            if (found == next.editor.nodes.end())
                next.editor.nodes.push_back(position);
            else
                *found = position;
        }
        return Apply("Move nodes", std::move(next));
    }
    std::vector<Uuid> GraphDocument::CopyNodes(std::span<const Uuid> ids, float offsetX, float offsetY)
    {
        const std::set<Uuid> selected(ids.begin(), ids.end());
        std::map<Uuid, Uuid> copies;
        std::vector<Uuid>    result;
        auto                 next = source;
        for (auto node : source.definition.nodes) {
            if (!selected.contains(node.id))
                continue;
            const auto original = node.id;
            node.id             = Uuid::Create();
            copies.emplace(original, node.id);
            result.push_back(node.id);
            next.definition.nodes.push_back(node);
            const auto position = std::ranges::find(source.editor.nodes, original, &GraphNodePosition::node);
            next.editor.nodes.push_back({node.id, offsetX + (position == source.editor.nodes.end() ? 0.f : position->x),
                                         offsetY + (position == source.editor.nodes.end() ? 0.f : position->y)});
        }
        for (auto link : source.definition.links) {
            if (copies.contains(link.from) && copies.contains(link.to)) {
                link.from = copies.at(link.from);
                link.to   = copies.at(link.to);
                next.definition.links.push_back(link);
            }
        }
        return Apply("Duplicate nodes", std::move(next)) ? result : std::vector<Uuid>{};
    }
} // namespace sky::animation::editor
