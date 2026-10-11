//
// ActionBarBuilder: one place that turns EditorActionRegistry entries into the chrome widgets,
// filtered by the active EditorScopeStack scope. The main window and dialogs both build their
// toolbar/menu from here, so the same actions render identically everywhere (see ui-standards.md).
//

#pragma once

#include <editor/shell/widgets/MenuBar.h>
#include <editor/shell/widgets/ToolBar.h>

#include <ui/UIEvent.h>

#include <functional>
#include <string>
#include <vector>

namespace sky::editor {

    class ActionBarBuilder {
    public:
        // Invokes an action by id (the host routes this through EditorShell::InvokeAction).
        using Invoker = std::function<void(const std::string &id)>;

        // Toolbar items for the active scope (ordered by group/order; separators between groups).
        static std::vector<ToolBar::Item> BuildToolBarItems(const Invoker &invoke);
        // Menus for the active scope (grouped by `menu`, nested by `submenu`).
        static std::vector<MenuBar::Menu> BuildMenus(const Invoker &invoke);
        // Resolves a key event to an action id in the active scope (empty when unmapped).
        static std::string MatchAccelerator(const sky::ui::UIKeyEvent &event);
    };

} // namespace sky::editor
