//
// Created on 2026/10/05.
//

#include <editor/shell/ReflectedInspectorPanel.h>

#include <editor/core/selection/SelectionService.h>

#include <ui/text/UITextSystem.h>

#include <utility>

namespace sky::editor {

    ReflectedInspectorPanel::ReflectedInspectorPanel(sky::ui::UITextSystem *text,
                                                     IEditorPropertySource *inSource,
                                                     SelectionService      *inSelection,
                                                     std::string            title)
        : ReflectedFormView(text, std::move(title)), source(inSource), selection(inSelection)
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
        Form().SetEditCommandFactory({});
        SetOnEdited({});
        if (source == nullptr || selection == nullptr || selection->IsEmpty()) {
            Bind(PropertyObject{});
            return;
        }
        for (const SelectionItem &item : selection->GetSelection()) {
            const std::vector<PropertyObject> objects = source->Resolve(item.id);
            if (!objects.empty()) {
                BindGroups(objects);
                Form().SetEditCommandFactory([owner = source, id = item.id](const PropertyDescriptor &property, Any value) {
                    return owner->MakeEditCommand(id, property, std::move(value));
                });
                SetOnEdited([this, id = item.id]() {
                    if (source)
                        source->OnEdited(id);
                });
                return;
            }
        }
        Bind(PropertyObject{});
    }

} // namespace sky::editor
