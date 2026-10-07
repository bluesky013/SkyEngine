//
// Created on 2026/10/07.
//

#pragma once

#include <core/environment/Singleton.h>

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace sky::editor {

    // A toolkit-agnostic editor action (toolbar / menu entry). Plugins register
    // actions through their EditorExtension; the shell renders them. No UI/render
    // type appears in this contract. One action can appear on the toolbar (by
    // `group`), in a menu (by `menu` / `submenu`), in both, or in neither.
    struct EditorAction {
        std::string           id;
        std::string           label;
        std::string           icon;          // icon name resolved by the UI; empty = text only
        std::string           group;         // toolbar section (e.g. "file"/"history"/"play"); empty = no toolbar
        std::string           menu;          // top-level menu label (e.g. "File"); empty = not in a menu
        std::string           submenu;       // optional second-level menu within `menu`
        int                   order     = 0; // order within the toolbar group / menu
        int                   menuOrder = 0; // top-level menu order
        std::function<bool()> enabled;       // null => always enabled
        std::function<void()> invoke;
    };

    // Process-wide registry (cross-DLL Singleton) of editor actions. The module /
    // plugins Add actions; the shell reads them to build the toolbar and menus.
    class EditorActionRegistry : public sky::Singleton<EditorActionRegistry> {
        friend class sky::Singleton<EditorActionRegistry>;

    public:
        // Adds or overrides an action by id.
        void Add(EditorAction action);
        void Clear();

        // Toolbar actions (non-empty `group`), sorted by (group, order, id).
        std::vector<EditorAction> GetToolbarActions() const;
        // Menu actions (non-empty `menu`), sorted by (menuOrder, menu, order, id).
        std::vector<EditorAction> GetMenuActions() const;
        const EditorAction       *Find(const std::string &id) const;

    private:
        std::unordered_map<std::string, EditorAction> actions;
    };

} // namespace sky::editor
