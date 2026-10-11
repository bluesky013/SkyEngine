//
// Created on 2026/10/07.
//

#include <editor/core/extension/EditorActionRegistry.h>

#include <algorithm>

namespace sky::editor {

    namespace {

        std::string ActionKey(const std::string &scope, const std::string &id)
        {
            return scope + '\x1f' + id;
        }

    } // namespace

    std::string EditorActionRegistry::ScopeOf(const EditorAction &action)
    {
        return action.scope.empty() ? std::string(EditorScopeStack::GlobalScope()) : action.scope;
    }

    const char *EditorMenuLabel(EditorMenu menu)
    {
        switch (menu) {
        case EditorMenu::File: return "File";
        case EditorMenu::Edit: return "Edit";
        case EditorMenu::View: return "View";
        case EditorMenu::Help: return "Help";
        case EditorMenu::Assets: return "Assets";
        case EditorMenu::None:
        default: return nullptr;
        }
    }

    void EditorActionRegistry::Add(EditorAction action)
    {
        if (action.id.empty()) {
            return;
        }
        if (action.scope.empty()) {
            action.scope = EditorScopeStack::GlobalScope();
        }
        actions[ActionKey(action.scope, action.id)] = std::move(action);
    }

    void EditorActionRegistry::Remove(const std::string &scope, const std::string &id)
    {
        actions.erase(ActionKey(scope, id));
    }

    void EditorActionRegistry::Clear()
    {
        actions.clear();
    }

    std::vector<EditorAction> EditorActionRegistry::GetActions() const
    {
        const std::string         active = EditorScopeStack::Get()->Active();
        std::vector<EditorAction> out;
        for (const auto &[key, action] : actions) {
            if (ScopeOf(action) == active) {
                out.push_back(action);
            }
        }
        std::sort(out.begin(), out.end(), [](const EditorAction &a, const EditorAction &b) { return a.id < b.id; });
        return out;
    }

    std::vector<EditorAction> EditorActionRegistry::GetToolbarActions() const
    {
        std::vector<EditorAction> out;
        for (const EditorAction &action : GetActions()) {
            if (action.group != EditorGroup::None) {
                out.push_back(action);
            }
        }
        std::sort(out.begin(), out.end(), [](const EditorAction &a, const EditorAction &b) {
            const int ga = static_cast<int>(a.group);
            const int gb = static_cast<int>(b.group);
            if (ga != gb) {
                return ga < gb;
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
        for (const EditorAction &action : GetActions()) {
            if (action.menu != EditorMenu::None) {
                out.push_back(action);
            }
        }
        std::sort(out.begin(), out.end(), [](const EditorAction &a, const EditorAction &b) {
            const int ma = static_cast<int>(a.menu);
            const int mb = static_cast<int>(b.menu);
            if (ma != mb) {
                return ma < mb;
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
        const std::string active = EditorScopeStack::Get()->Active();
        if (const auto it = actions.find(ActionKey(active, id)); it != actions.end()) {
            return &it->second;
        }
        if (active != EditorScopeStack::GlobalScope()) {
            if (const auto it = actions.find(ActionKey(EditorScopeStack::GlobalScope(), id)); it != actions.end()) {
                return &it->second;
            }
        }
        return nullptr;
    }

} // namespace sky::editor
