//
// Created on 2026/10/12.
//

#pragma once

#include <editor/widgets/ReflectedFormView.h>

#include <functional>
#include <string>

namespace sky::ui {
    class UITextSystem;
} // namespace sky::ui

namespace sky {
    struct ExportPresets;
}

namespace sky::editor {

    // Dockable "Export Presets" panel: a reflected form bound to the project's
    // `ExportPresets` document (the sequence member renders every preset). Edits go
    // through the shared reflection form + CommandService; the host persists via
    // the onEdited callback. No hand-written per-field widgets.
    class PresetEditorPanel : public ReflectedFormView {
    public:
        using Provider = std::function<sky::ExportPresets *()>;

        PresetEditorPanel(sky::ui::UITextSystem *text, Provider provider, std::function<void()> onEdited, std::string title);

        const char *GetTypeName() const override
        {
            return "PresetEditorPanel";
        }

    private:
        void Rebind();

        Provider provider;
    };

} // namespace sky::editor
