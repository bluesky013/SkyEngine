//
// Created on 2026/10/04.
//

#include <editor/shell/ReflectedWidget.h>

namespace sky::editor {

    ReflectedWidgetRegistry &ReflectedWidgetRegistry::Get()
    {
        static ReflectedWidgetRegistry registry;
        return registry;
    }

    void ReflectedWidgetRegistry::Register(PropertyEditorKind kind, std::shared_ptr<ReflectedWidget> widget)
    {
        if (widget) {
            widgets[static_cast<uint8_t>(kind)] = std::move(widget);
        }
    }

    ReflectedWidget *ReflectedWidgetRegistry::Find(PropertyEditorKind kind) const
    {
        const auto iter = widgets.find(static_cast<uint8_t>(kind));
        return iter == widgets.end() ? nullptr : iter->second.get();
    }

    ReflectedWidget *ReflectedWidgetRegistry::FindWithPopup() const
    {
        for (const auto &entry : widgets) {
            if (entry.second && entry.second->HasPopup()) {
                return entry.second.get();
            }
        }
        return nullptr;
    }

} // namespace sky::editor
