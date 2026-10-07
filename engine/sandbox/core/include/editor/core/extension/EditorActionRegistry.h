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
    // type appears in this contract, so a subsystem plugin can contribute a
    // toolbar button without depending on the shell.
    struct EditorAction {
        std::string           id;
        std::string           label;
        std::string           icon;  // icon name resolved by the UI; empty = text only
        std::string           group; // toolbar section (e.g. "file", "history", "play")
        int                   order = 0;
        std::function<bool()> enabled; // null => always enabled
        std::function<void()> invoke;
    };

    // Process-wide registry (cross-DLL Singleton) of editor actions. The module /
    // plugins Add actions; the shell reads them to build the toolbar (and, later,
    // the menu).
    class EditorActionRegistry : public sky::Singleton<EditorActionRegistry> {
        friend class sky::Singleton<EditorActionRegistry>;

    public:
        // Adds or overrides an action by id.
        void Add(EditorAction action);
        void Clear();

        std::vector<EditorAction> GetActions() const; // sorted by (group, order, id)

    private:
        std::unordered_map<std::string, EditorAction> actions;
    };

} // namespace sky::editor
