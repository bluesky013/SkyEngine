//
// Created on 2026/10/07.
//

#include <editor/core/extension/EditorActionRegistry.h>

#include <algorithm>

namespace sky::editor {

    void EditorActionRegistry::Add(EditorAction action)
    {
        if (action.id.empty()) {
            return;
        }
        actions[action.id] = std::move(action);
    }

    void EditorActionRegistry::Clear()
    {
        actions.clear();
    }

    std::vector<EditorAction> EditorActionRegistry::GetActions() const
    {
        std::vector<EditorAction> out;
        out.reserve(actions.size());
        for (const auto &[id, action] : actions) {
            out.push_back(action);
        }
        std::sort(out.begin(), out.end(), [](const EditorAction &a, const EditorAction &b) {
            if (a.group != b.group) {
                return a.group < b.group;
            }
            if (a.order != b.order) {
                return a.order < b.order;
            }
            return a.id < b.id;
        });
        return out;
    }

} // namespace sky::editor
