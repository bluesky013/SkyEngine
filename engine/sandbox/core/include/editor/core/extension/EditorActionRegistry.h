//
// Created on 2026/10/07.
//

#pragma once

#include <core/environment/Singleton.h>
#include <editor/core/extension/EditorScopeStack.h>

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace sky::editor {

    // Top-level menu identity; ordering follows the declaration order.
    enum class EditorMenu : uint8_t {
        None = 0,
        File,
        Edit,
        View,
        Help,
        Assets,
    };

    // Toolbar group identity (section + ordering); ordering follows the declaration order.
    enum class EditorGroup : uint8_t {
        None = 0,
        File,
        History,
        Play,
    };

    // Display label for a menu ("File", "Edit", ...); nullptr for EditorMenu::None.
    const char *EditorMenuLabel(EditorMenu menu);

    // A toolkit-agnostic editor action (toolbar / menu entry / shortcut). Plugins register
    // actions through their EditorExtension; the shell renders them. No UI/render type appears
    // in this contract. One action can appear on the toolbar (by `group`), in a menu (by `menu` /
    // `submenu`), in both, or in neither.
    struct EditorAction {
        std::string           id;
        std::string           label;
        std::string           icon;                      // icon name resolved by the UI; empty = text only
        EditorGroup           group = EditorGroup::None; // toolbar section; None = not on the toolbar
        EditorMenu            menu  = EditorMenu::None;  // top-level menu; None = not in a menu
        std::string           submenu;                   // optional second-level menu within `menu`
        int                   order = 0;                 // order within the toolbar group / menu
        std::function<bool()> enabled;                   // null => always enabled
        std::function<void()> invoke;
        // Chrome scope: empty/"global" = the main-window chrome. A dialog/viewer pushes its scope
        // (EditorScopeStack) so its actions drive the chrome while it is active; actions only
        // render/resolve in their own scope.
        std::string scope;
        // Data-driven accelerator, e.g. "Ctrl+S", "Ctrl+Shift+Z"; comma-separates alternatives.
        std::string accelerator;
    };

    // Process-wide registry (cross-DLL Singleton) of editor actions, keyed by (scope, id). The
    // module / plugins Add actions; the shell reads them to build the toolbar and menus for the
    // active scope. Handler resolution follows the active scope (see EditorScopeStack).
    class EditorActionRegistry : public sky::Singleton<EditorActionRegistry> {
        friend class sky::Singleton<EditorActionRegistry>;

    public:
        // Adds or overrides an action by (scope, id). An empty scope means "global".
        void Add(EditorAction action);
        void Remove(const std::string &scope, const std::string &id);
        void Clear();

        // Actions in the active scope, sorted by id (for accelerator matching).
        std::vector<EditorAction> GetActions() const;
        // Active-scope toolbar actions (non-empty `group`), sorted by (group, order, id).
        std::vector<EditorAction> GetToolbarActions() const;
        // Active-scope menu actions (non-empty `menu`), sorted by (menuOrder, menu, order, id).
        std::vector<EditorAction> GetMenuActions() const;
        // Resolves by active scope first, then global.
        const EditorAction *Find(const std::string &id) const;

        // The scope an action belongs to ("global" when unset).
        static std::string ScopeOf(const EditorAction &action);

    private:
        std::unordered_map<std::string, EditorAction> actions; // key(scope, id) -> action
    };

} // namespace sky::editor
