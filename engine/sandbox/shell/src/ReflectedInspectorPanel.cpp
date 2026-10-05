//
// Created on 2026/10/05.
//

#include <editor/shell/ReflectedInspectorPanel.h>

#include <editor/core/selection/SelectionService.h>

#include <ui/text/UITextSystem.h>

#include <utility>

namespace sky::editor {

    ReflectedInspectorPanel::ReflectedInspectorPanel(sky::ui::UITextSystem *text, IEditorPropertySource *inSource,
                                                     SelectionService *inSelection, std::string title)
        : ReflectedFormView(text, std::move(title))
        , source(inSource)
        , selection(inSelection)
    {
        if (selection != nullptr) {
            callbackId = selection->AddChangedCallback([this]() { ResolveSelection(); });
        }
        ResolveSelection();
    }

    ReflectedInspectorPanel::~ReflectedInspectorPanel()
    {
        if (selection != nullptr && callbackId != 0) {
            selection->RemoveChangedCallback(callbackId);
        }
    }

    void ReflectedInspectorPanel::ResolveSelection()
    {
        if (source == nullptr || selection == nullptr || selection->IsEmpty()) {
            Bind(PropertyObject{});
            return;
        }
        for (const SelectionItem &item : selection->GetSelection()) {
            const std::vector<PropertyObject> objects = source->Resolve(item.id);
            if (!objects.empty()) {
                Bind(objects.front());
                return;
            }
        }
        Bind(PropertyObject{});
    }

} // namespace sky::editor
