//
// Created on 2026/09/21.
//

#pragma once

#include <cstdint>
#include <string>

namespace sky::editor {

    // Toolkit-independent input routing state.
    //
    // Tracks which panel has focus and whether the UI surface currently wants
    // input, so the viewport can be gated while UI is active.
    class InputRouter {
    public:
        InputRouter() = default;
        ~InputRouter() = default;

        void SetFocus(std::string panelId);
        void ClearFocus();
        const std::string &GetFocusPanel() const { return focusPanel; }
        bool HasFocus() const { return !focusPanel.empty(); }

        // Whether the focused panel consumes text/key input (for example a text
        // field). Reset when focus changes.
        void SetFocusConsumesInput(bool consumes) { focusConsumesInput = consumes; }
        bool GetFocusConsumesInput() const { return focusConsumesInput; }

        void PushModal();
        void PopModal();
        uint32_t GetModalDepth() const { return modalDepth; }

        // True when the UI wants input and the viewport should not receive it.
        bool WantsInput() const { return modalDepth > 0 || (HasFocus() && focusConsumesInput); }

    private:
        std::string focusPanel;
        bool focusConsumesInput = false;
        uint32_t modalDepth = 0;
    };

} // namespace sky::editor
