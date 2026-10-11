//
// Created on 2026/10/12.
//

#include <editor/shell/PresetEditorPanel.h>

#include <framework/project/ExportPreset.h>
#include <framework/serialization/SerializationContext.h>

#include <utility>

namespace sky::editor {

    PresetEditorPanel::PresetEditorPanel(sky::ui::UITextSystem *text,
                                         Provider               inProvider,
                                         std::function<void()>  onEdited,
                                         std::string            title)
        : ReflectedFormView(text, std::move(title)), provider(std::move(inProvider))
    {
        sky::RegisterExportPresetReflection();
        Rebind();
        SetOnEdited(std::move(onEdited));
    }

    void PresetEditorPanel::Rebind()
    {
        sky::ExportPresets *document = provider ? provider() : nullptr;
        if (document == nullptr) {
            Bind(PropertyObject{});
            return;
        }
        const sky::TypeNode *node = sky::GetTypeNode(sky::TypeInfo<sky::ExportPresets>::RegisteredId());
        if (node == nullptr) {
            Bind(PropertyObject{});
            return;
        }
        Bind(PropertyObject{document, node});
    }

} // namespace sky::editor
