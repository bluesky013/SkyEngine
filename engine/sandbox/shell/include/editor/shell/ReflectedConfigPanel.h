//
// Created on 2026/10/05.
//

#pragma once

#include <editor/shell/ReflectedFormView.h>
#include <editor/core/property/EditorPropertySource.h>

namespace sky::ui {
    class UITextSystem;
} // namespace sky::ui

namespace sky::editor {

    // Global config panel: renders named reflected configurations obtained from an
    // IEditorConfigSource through the reflected-form framework.
    class ReflectedConfigPanel : public ReflectedFormView {
    public:
        ReflectedConfigPanel(sky::ui::UITextSystem *text, IEditorConfigSource *source, std::string title);

        const char *GetTypeName() const override { return "ReflectedConfigPanel"; }

        void OnViewTick() override;

    private:
        void ResolveConfigs();

        IEditorConfigSource *configSource = nullptr;
    };

} // namespace sky::editor
