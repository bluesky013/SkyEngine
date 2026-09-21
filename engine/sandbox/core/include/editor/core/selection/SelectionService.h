//
// Created on 2026/09/21.
//

#pragma once

#include <cstdint>
#include <core/util/Uuid.h>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace sky::editor {

    enum class SelectionType : uint8_t {
        NONE,
        ENTITY,
        ASSET,
    };

    struct SelectionItem {
        SelectionType type = SelectionType::NONE;
        Uuid id;
    };

    // Toolkit-independent owner of the editor selection and active context.
    //
    // The outliner, inspector, and viewport observe this service rather than
    // holding independent copies, so a change in one view is visible to all.
    class SelectionService {
    public:
        using ChangeCallback = std::function<void()>;
        using CallbackId = uint32_t;

        SelectionService() = default;
        ~SelectionService() = default;

        SelectionService(const SelectionService &) = delete;
        SelectionService &operator=(const SelectionService &) = delete;

        void SetSelection(std::vector<SelectionItem> newSelection);
        void Clear();

        bool IsEmpty() const { return selection.empty(); }
        const std::vector<SelectionItem> &GetSelection() const { return selection; }

        void SetContext(std::string newContext);
        const std::string &GetContext() const { return context; }

        CallbackId AddChangedCallback(ChangeCallback callback);
        void RemoveChangedCallback(CallbackId id);

    private:
        static bool SameSelection(const std::vector<SelectionItem> &lhs, const std::vector<SelectionItem> &rhs);
        void Notify();

        std::vector<SelectionItem> selection;
        std::string context;
        std::unordered_map<CallbackId, ChangeCallback> callbacks;
        CallbackId nextCallbackId = 1;
    };

} // namespace sky::editor
