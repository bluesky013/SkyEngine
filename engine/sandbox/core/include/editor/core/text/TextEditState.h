//
// Created on 2026/10/06.
//

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace sky::editor {

    // Headless single-line text editing state: text, caret, and an anchor-based
    // selection. Key handling uses virtual-key codes (the convention used by the
    // engine UI widgets). Control characters are rejected on insert, so platform
    // WM_CHAR codes for Backspace/Enter/Tab/Esc never enter the buffer.
    class TextEditState {
    public:
        void               SetText(std::string value);
        const std::string &GetText() const
        {
            return text;
        }

        std::size_t GetCaret() const
        {
            return caret;
        }
        bool HasSelection() const
        {
            return anchor != caret;
        }
        std::size_t GetSelectionStart() const;
        std::size_t GetSelectionEnd() const;

        // Places the caret and clears the selection.
        void SetCaret(std::size_t index);
        void SelectAll();

        // Replaces the selection (if any) with `value`, dropping control chars.
        void Insert(const std::string &value);
        void DeleteSelection();
        void EraseBackward();
        void EraseForward();

        void MoveLeft(bool select);
        void MoveRight(bool select);
        void MoveHome(bool select);
        void MoveEnd(bool select);

        // Returns true if the key was consumed. `shift` extends the selection.
        bool OnKey(std::uint32_t vk, bool shift, bool ctrl);
        // Returns true if printable text was inserted.
        bool OnText(const std::string &value);

        static bool IsPrintable(const std::string &value);

    private:
        std::string text;
        std::size_t caret  = 0;
        std::size_t anchor = 0;
    };

} // namespace sky::editor
