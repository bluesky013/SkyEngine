//
// Created on 2026/10/06.
//

#pragma once

#include <ui/UIEvent.h>

#include <memory>

namespace sky::editor {

    class EditorShell;
    class ProjectManagerView;

    // Uniform input sink for the editor's UI hosts. The module selects one active
    // target (hub view or editor shell) instead of branching per event, which also
    // makes routing to floating surfaces a target concern rather than the module's.
    class IUiTarget {
    public:
        virtual ~IUiTarget() = default;

        virtual void OnPointer(const sky::ui::UIPointerEvent &event) = 0;
        virtual void OnKey(const sky::ui::UIKeyEvent &event) {}
        virtual void OnText(const sky::ui::UITextInputEvent &event) {}
        virtual bool WantsInput() const { return false; }
    };

    std::unique_ptr<IUiTarget> MakeHubTarget(ProjectManagerView *view);
    std::unique_ptr<IUiTarget> MakeShellTarget(EditorShell *shell);

} // namespace sky::editor
