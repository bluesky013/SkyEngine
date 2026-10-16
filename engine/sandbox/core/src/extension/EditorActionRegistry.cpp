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
        const std::string key = ActionKey(action.scope, action.id);
        actions[key]          = Entry{std::move(action), ++revision};
    }

    void EditorActionRegistry::Remove(const std::string &scope, const std::string &id)
    {
        if (actions.erase(ActionKey(scope.empty() ? EditorScopeStack::GlobalScope() : scope, id)) != 0) {
            ++revision;
        }
    }

    void EditorActionRegistry::Clear()
    {
        actions.clear();
        ++revision;
    }

    std::vector<EditorAction> EditorActionRegistry::GetActions() const
    {
        const std::string         active = EditorScopeStack::Get()->Active();
        std::vector<EditorAction> out;
        for (const auto &[key, entry] : actions) {
            const auto &action = entry.action;
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
            return &it->second.action;
        }
        if (active != EditorScopeStack::GlobalScope()) {
            if (const auto it = actions.find(ActionKey(EditorScopeStack::GlobalScope(), id)); it != actions.end()) {
                return &it->second.action;
            }
        }
        return nullptr;
    }

    EditorActionSnapshot EditorActionRegistry::GetSnapshot(const std::string &id) const
    {
        const EditorAction *action = Find(id);
        if (action == nullptr) {
            return {};
        }
        const auto it = actions.find(ActionKey(ScopeOf(*action), id));
        return {it->second.action, it->second.generation, EditorScopeStack::Get()->Active()};
    }

    std::vector<EditorActionSnapshot> EditorActionRegistry::GetActionSnapshots() const
    {
        const std::string                 active = EditorScopeStack::Get()->Active();
        std::vector<EditorActionSnapshot> out;
        for (const auto &[key, entry] : actions) {
            if (ScopeOf(entry.action) == active) {
                out.push_back({entry.action, entry.generation, active});
            }
        }
        std::sort(out.begin(), out.end(), [](const auto &a, const auto &b) { return a.action.id < b.action.id; });
        return out;
    }

    bool EditorActionRegistry::IsCurrent(const EditorActionSnapshot &snapshot) const
    {
        if (snapshot.generation == 0 || snapshot.activeScope != EditorScopeStack::Get()->Active()) {
            return false;
        }
        const auto it = actions.find(ActionKey(ScopeOf(snapshot.action), snapshot.action.id));
        if (it == actions.end() || it->second.generation != snapshot.generation) {
            return false;
        }
        // Preserve Find's existing global fallback, while rejecting chrome from another scope.
        const auto *resolved = Find(snapshot.action.id);
        return resolved != nullptr && ScopeOf(*resolved) == ScopeOf(snapshot.action);
    }

    EditorActionState EditorActionRegistry::ResolveState(const EditorActionSnapshot &snapshot) const
    {
        const EditorActionSnapshot current = snapshot;
        const auto                 stale   = []() { return EditorActionState{false, false, false, false}; };
        if (!IsCurrent(current)) {
            return stale();
        }
        // Copy before calling user code: it may remove or replace the registration (or its owner).
        const EditorAction &action = current.action;
        EditorActionState   state;
        state.checkable = action.checkable;
        state.visible   = !action.visible || action.visible();
        if (!IsCurrent(current) || !state.visible) {
            return stale();
        }
        state.enabled = !action.enabled || action.enabled();
        if (!IsCurrent(current)) {
            return stale();
        }
        state.checked = state.checkable && action.checked && action.checked();
        return IsCurrent(current) ? state : stale();
    }

    bool EditorActionRegistry::Invoke(const EditorActionSnapshot &snapshot, const std::function<void()> &dispatcher) const
    {
        const EditorActionSnapshot  current  = snapshot;
        const std::function<void()> dispatch = dispatcher;
        const EditorActionState     state    = ResolveState(current);
        if (!state.visible || !IsCurrent(current)) {
            return false;
        }
        if (!state.enabled) {
            return true;
        }
        const std::function<void()> invoke = dispatch ? dispatch : current.action.invoke;
        if (invoke) {
            invoke();
        }
        return true;
    }

    bool EditorActionRegistry::Invoke(const std::string &id) const
    {
        return Invoke(GetSnapshot(id));
    }

} // namespace sky::editor
