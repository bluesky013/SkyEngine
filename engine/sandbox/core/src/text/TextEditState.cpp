//
// Created on 2026/10/06.
//

#include <editor/core/text/TextEditState.h>

#include <algorithm>
#include <utility>

namespace sky::editor {

    namespace {
        constexpr std::uint32_t VK_BACK   = 0x08;
        constexpr std::uint32_t VK_END    = 0x23;
        constexpr std::uint32_t VK_HOME   = 0x24;
        constexpr std::uint32_t VK_LEFT   = 0x25;
        constexpr std::uint32_t VK_RIGHT  = 0x27;
        constexpr std::uint32_t VK_DELETE = 0x2E;
        constexpr std::uint32_t VK_A      = 0x41;

        std::string FilterPrintable(const std::string &value)
        {
            std::string out;
            out.reserve(value.size());
            for (char c : value) {
                const unsigned char uc = static_cast<unsigned char>(c);
                if (uc >= 0x20 && uc != 0x7F) {
                    out.push_back(c);
                }
            }
            return out;
        }
    } // namespace

    void TextEditState::SetText(std::string value)
    {
        text   = std::move(value);
        caret  = text.size();
        anchor = caret;
    }

    std::size_t TextEditState::GetSelectionStart() const
    {
        return std::min(anchor, caret);
    }

    std::size_t TextEditState::GetSelectionEnd() const
    {
        return std::max(anchor, caret);
    }

    void TextEditState::SetCaret(std::size_t index)
    {
        caret  = std::min(index, text.size());
        anchor = caret;
    }

    void TextEditState::SelectAll()
    {
        anchor = 0;
        caret  = text.size();
    }

    bool TextEditState::IsPrintable(const std::string &value)
    {
        return !FilterPrintable(value).empty();
    }

    void TextEditState::DeleteSelection()
    {
        if (!HasSelection()) {
            return;
        }
        const std::size_t start = GetSelectionStart();
        const std::size_t end   = GetSelectionEnd();
        text.erase(start, end - start);
        caret  = start;
        anchor = start;
    }

    void TextEditState::Insert(const std::string &value)
    {
        const std::string printable = FilterPrintable(value);
        if (printable.empty()) {
            return;
        }
        DeleteSelection();
        text.insert(caret, printable);
        caret += printable.size();
        anchor = caret;
    }

    void TextEditState::EraseBackward()
    {
        if (HasSelection()) {
            DeleteSelection();
            return;
        }
        if (caret == 0) {
            return;
        }
        text.erase(caret - 1, 1);
        --caret;
        anchor = caret;
    }

    void TextEditState::EraseForward()
    {
        if (HasSelection()) {
            DeleteSelection();
            return;
        }
        if (caret >= text.size()) {
            return;
        }
        text.erase(caret, 1);
        anchor = caret;
    }

    void TextEditState::MoveLeft(bool select)
    {
        if (!select) {
            caret  = HasSelection() ? GetSelectionStart() : (caret > 0 ? caret - 1 : 0);
            anchor = caret;
        } else if (caret > 0) {
            --caret;
        }
    }

    void TextEditState::MoveRight(bool select)
    {
        if (!select) {
            caret  = HasSelection() ? GetSelectionEnd() : std::min(caret + 1, text.size());
            anchor = caret;
        } else if (caret < text.size()) {
            ++caret;
        }
    }

    void TextEditState::MoveHome(bool select)
    {
        caret = 0;
        if (!select) {
            anchor = 0;
        }
    }

    void TextEditState::MoveEnd(bool select)
    {
        caret = text.size();
        if (!select) {
            anchor = caret;
        }
    }

    bool TextEditState::OnKey(std::uint32_t vk, bool shift, bool ctrl)
    {
        if (ctrl && (vk == VK_A || vk == VK_A + 32)) {
            SelectAll();
            return true;
        }
        switch (vk) {
        case VK_BACK: EraseBackward(); return true;
        case VK_DELETE: EraseForward(); return true;
        case VK_LEFT: MoveLeft(shift); return true;
        case VK_RIGHT: MoveRight(shift); return true;
        case VK_HOME: MoveHome(shift); return true;
        case VK_END: MoveEnd(shift); return true;
        default: return false;
        }
    }

    bool TextEditState::OnText(const std::string &value)
    {
        if (!IsPrintable(value)) {
            return false;
        }
        Insert(value);
        return true;
    }

} // namespace sky::editor
