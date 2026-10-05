//
// Created on 2026/10/05.
//

#pragma once

#include <editor/shell/ReflectedFormView.h>
#include <editor/core/property/EditorPropertySource.h>

#include <cstdint>

namespace sky::ui {
    class UITextSystem;
} // namespace sky::ui

namespace sky::editor {

    class SelectionService;

    // Inspector panel: binds the current selection (resolved through an
    // IEditorPropertySource) to a reflected-form view. Empty when unresolved.
    class ReflectedInspectorPanel : public ReflectedFormView {
    public:
        ReflectedInspectorPanel(sky::ui::UITextSystem *text, IEditorPropertySource *source,
                                SelectionService *selection, std::string title);
        ~ReflectedInspectorPanel() override;

        const char *GetTypeName() const override { return "ReflectedInspectorPanel"; }

    private:
        void ResolveSelection();

        IEditorPropertySource *source = nullptr;
        SelectionService      *selection = nullptr;
        uint32_t               callbackId = 0;
    };

} // namespace sky::editor
