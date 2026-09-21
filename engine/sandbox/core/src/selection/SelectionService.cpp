//
// Created on 2026/09/21.
//

#include <editor/core/selection/SelectionService.h>
#include <utility>

namespace sky::editor {

    bool SelectionService::SameSelection(const std::vector<SelectionItem> &lhs, const std::vector<SelectionItem> &rhs)
    {
        if (lhs.size() != rhs.size()) {
            return false;
        }
        for (size_t i = 0; i < lhs.size(); ++i) {
            if (lhs[i].type != rhs[i].type || !(lhs[i].id == rhs[i].id)) {
                return false;
            }
        }
        return true;
    }

    void SelectionService::SetSelection(std::vector<SelectionItem> newSelection)
    {
        if (SameSelection(selection, newSelection)) {
            return;
        }
        selection = std::move(newSelection);
        Notify();
    }

    void SelectionService::Clear()
    {
        if (selection.empty()) {
            return;
        }
        selection.clear();
        Notify();
    }

    void SelectionService::SetContext(std::string newContext)
    {
        if (context == newContext) {
            return;
        }
        context = std::move(newContext);
        Notify();
    }

    SelectionService::CallbackId SelectionService::AddChangedCallback(ChangeCallback callback)
    {
        const CallbackId id = nextCallbackId++;
        callbacks.emplace(id, std::move(callback));
        return id;
    }

    void SelectionService::RemoveChangedCallback(CallbackId id)
    {
        callbacks.erase(id);
    }

    void SelectionService::Notify()
    {
        // Snapshot so callbacks may safely add or remove observers.
        const auto snapshot = callbacks;
        for (const auto &[id, callback] : snapshot) {
            if (callback) {
                callback();
            }
        }
    }

} // namespace sky::editor
