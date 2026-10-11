//
// EditorScopeStack: process-wide (cross-DLL) stack of active editor chrome scopes.
//
// The main window's chrome runs in the "global" scope. A dialog/viewer pushes its own scope
// while it is open (top of stack = active); the toolbar/menu/shortcut builders resolve actions
// whose scope equals the active scope, so the same EditorActionRegistry entries drive the
// dialog's chrome without a per-dialog command set. See docs/editor/ui-layout-and-chrome-plan.md.
//

#pragma once

#include <core/environment/Singleton.h>

#include <algorithm>
#include <string>
#include <vector>

namespace sky::editor {

    class EditorScopeStack : public sky::Singleton<EditorScopeStack> {
        friend class sky::Singleton<EditorScopeStack>;

    public:
        // The scope the main-window chrome runs in (and the default for actions with no scope).
        static const char *GlobalScope()
        {
            return "global";
        }

        void Push(const std::string &scope)
        {
            stack.push_back(scope);
        }
        // Removes the most recent occurrence of `scope` (tolerates out-of-order closes).
        void Pop(const std::string &scope)
        {
            const auto it = std::find(stack.rbegin(), stack.rend(), scope);
            if (it != stack.rend()) {
                stack.erase(std::next(it).base());
            }
        }
        // The active scope: the top of the stack, or "global" when empty.
        std::string Active() const
        {
            return stack.empty() ? std::string(GlobalScope()) : stack.back();
        }

        void Clear()
        {
            stack.clear();
        }
        bool IsEmpty() const
        {
            return stack.empty();
        }

    private:
        std::vector<std::string> stack;
    };

} // namespace sky::editor
