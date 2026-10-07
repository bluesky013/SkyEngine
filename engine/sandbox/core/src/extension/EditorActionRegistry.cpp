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

    std::vector<EditorAction> EditorActionRegistry::GetToolbarActions() const
    {
        std::vector<EditorAction> out;
        for (const auto &[id, action] : actions) {
            if (!action.group.empty()) {
                out.push_back(action);
            }
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

    std::vector<EditorAction> EditorActionRegistry::GetMenuActions() const
    {
        std::vector<EditorAction> out;
        for (const auto &[id, action] : actions) {
            if (!action.menu.empty()) {
                out.push_back(action);
            }
        }
        std::sort(out.begin(), out.end(), [](const EditorAction &a, const EditorAction &b) {
            if (a.menuOrder != b.menuOrder) {
                return a.menuOrder < b.menuOrder;
            }
            if (a.menu != b.menu) {
                return a.menu < b.menu;
            }
            if (a.order != b.order) {
                return a.order < b.order;
            }
            return a.id < b.id;
        });
        return out;
    }

    const EditorAction *EditorActionRegistry::Find(const std::string &id) const
    {
        const auto it = actions.find(id);
        return it != actions.end() ? &it->second : nullptr;
    }

} // namespace sky::editor
