//
// Created on 2026/10/04.
//

#include <editor/widgets/ReflectedWidget.h>

namespace sky::editor {

    void ReflectedWidgetRegistry::Register(PropertyEditorKind kind, std::shared_ptr<ReflectedWidget> widget)
    {
        if (widget) {
            widgets[static_cast<uint8_t>(kind)] = std::move(widget);
            const auto notify                   = onChanged;
            if (notify) {
                notify();
            }
        }
    }

    ReflectedWidget *ReflectedWidgetRegistry::Find(PropertyEditorKind kind) const
    {
        return FindShared(kind).get();
    }

    ReflectedWidget *ReflectedWidgetRegistry::FindWithPopup() const
    {
        return FindPopupShared().get();
    }

    std::shared_ptr<ReflectedWidget> ReflectedWidgetRegistry::FindShared(PropertyEditorKind kind) const
    {
        const auto iter = widgets.find(static_cast<uint8_t>(kind));
        return iter == widgets.end() ? nullptr : iter->second;
    }

    std::shared_ptr<ReflectedWidget> ReflectedWidgetRegistry::FindPopupShared() const
    {
        for (const auto &entry : widgets) {
            if (entry.second && entry.second->HasPopup()) {
                return entry.second;
            }
        }
        return nullptr;
    }

    bool ReflectedWidgetRegistry::IsInteracting() const
    {
        for (const auto &entry : widgets) {
            if (entry.second && entry.second->IsInteracting()) {
                return true;
            }
        }
        return false;
    }

    void
    ReflectedWidgetRegistry::ResetInteractions(ReflectedWidgetHost &host, ReflectedInteractionReset mode, std::shared_ptr<ReflectedWidget> active)
    {
        // Generic kinds can share one instance inside a form. Reset it only
        // once, including when a reset callback updates another registration.
        std::unordered_map<ReflectedWidget *, std::shared_ptr<ReflectedWidget>> instances;
        if (active) {
            ReflectedWidget *raw = active.get();
            instances.emplace(raw, std::move(active));
        }
        for (const auto &entry : widgets) {
            if (entry.second) {
                instances.emplace(entry.second.get(), entry.second);
            }
        }
        for (const auto &entry : instances) {
            entry.second->ResetInteraction(host, mode);
        }
    }

} // namespace sky::editor
