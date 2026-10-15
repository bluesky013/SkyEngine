//
// Created on 2026/10/15.
//

#include "AndroidInput.h"

#include <algorithm>

namespace sky {

    void AndroidLifecycleState::Apply(AndroidLifecycleEvent event)
    {
        if (destroyed) {
            return;
        }
        const bool wasResumed = resumed;
        const bool hadSurface = surface;
        switch (event) {
        case AndroidLifecycleEvent::RESUME: resumed = true; break;
        case AndroidLifecycleEvent::PAUSE: resumed = false; break;
        case AndroidLifecycleEvent::FOCUS_GAINED: focused = true; break;
        case AndroidLifecycleEvent::FOCUS_LOST: focused = false; break;
        case AndroidLifecycleEvent::SURFACE_AVAILABLE: surface = true; break;
        case AndroidLifecycleEvent::SURFACE_LOST: surface = false; break;
        case AndroidLifecycleEvent::DESTROY:
            destroyed = true;
            surface   = false;
            resumed   = false;
            focused   = false;
            break;
        }
        if (wasResumed != resumed || hadSurface != surface || destroyed) {
            ++epoch;
        }
    }

    bool AndroidLifecycleState::IsFrameActive() const
    {
        return resumed && surface && !destroyed;
    }

    bool AndroidLifecycleState::HasFocus() const
    {
        return focused && !destroyed;
    }

    bool AndroidLifecycleState::IsDestroyed() const
    {
        return destroyed;
    }

    uint64_t AndroidLifecycleState::GetEpoch() const
    {
        return epoch;
    }

    void AndroidPointerState::UpdateButtons(uint32_t next, std::vector<AndroidMouseChange> &changes)
    {
        const MouseButtonType mapped[] = {MouseButtonType::LEFT, MouseButtonType::RIGHT, MouseButtonType::MIDDLE};
        for (uint32_t index = 0; index != 3; ++index) {
            const uint32_t bit = 1U << index;
            if ((buttons & bit) != (next & bit)) {
                AndroidMouseChange change;
                change.type   = (next & bit) != 0 ? AndroidMouseChange::Type::DOWN : AndroidMouseChange::Type::UP;
                change.button = mapped[index];
                change.x      = x;
                change.y      = y;
                changes.push_back(change);
            }
        }
        buttons = next & 7U;
    }

    std::vector<AndroidMouseChange> AndroidPointerState::Cancel()
    {
        std::vector<AndroidMouseChange> changes;
        UpdateButtons(0, changes);
        primary    = -1;
        positioned = false;
        return changes;
    }

    std::vector<AndroidMouseChange> AndroidPointerState::Apply(const AndroidMotionSnapshot &motion)
    {
        if (motion.action == AndroidMotionAction::CANCEL) {
            return Cancel();
        }
        std::vector<AndroidMouseChange> changes;
        if (motion.pointers.empty() || motion.actionIndex >= motion.pointers.size()) {
            return changes;
        }
        const AndroidPointerPosition *pointer = &motion.pointers.front();
        if (motion.touch) {
            if (motion.action == AndroidMotionAction::DOWN) {
                changes = Cancel();
                primary = motion.pointers[motion.actionIndex].id;
            }
            const auto iter = std::find_if(motion.pointers.begin(), motion.pointers.end(),
                                           [this](const AndroidPointerPosition &value) { return value.id == primary; });
            if (iter == motion.pointers.end()) {
                return changes;
            }
            if ((motion.action == AndroidMotionAction::POINTER_DOWN || motion.action == AndroidMotionAction::POINTER_UP) &&
                motion.pointers[motion.actionIndex].id != primary) {
                return changes;
            }
            pointer = &*iter;
        }
        if (!positioned || x != pointer->x || y != pointer->y) {
            AndroidMouseChange change;
            change.x    = pointer->x;
            change.y    = pointer->y;
            change.relX = positioned ? pointer->x - x : 0;
            change.relY = positioned ? pointer->y - y : 0;
            changes.push_back(change);
        }
        x          = pointer->x;
        y          = pointer->y;
        positioned = true;
        if (motion.touch) {
            if (motion.action == AndroidMotionAction::DOWN) {
                UpdateButtons(1, changes);
            } else if (motion.action == AndroidMotionAction::UP || motion.action == AndroidMotionAction::POINTER_UP) {
                UpdateButtons(0, changes);
                primary    = -1;
                positioned = false;
            }
        } else {
            UpdateButtons(motion.buttons, changes);
        }
        if (motion.action == AndroidMotionAction::SCROLL && motion.scroll != 0.f) {
            AndroidMouseChange change;
            change.type   = AndroidMouseChange::Type::WHEEL;
            change.x      = x;
            change.y      = y;
            change.scroll = motion.scroll;
            changes.push_back(change);
        }
        return changes;
    }

    ScanCode AndroidKeyCodeToScanCode(int32_t keyCode)
    {
        // Stable Android KeyEvent keyCode values; deliberately independent of Linux evdev scan codes.
        if (keyCode >= 29 && keyCode <= 54) {
            return static_cast<ScanCode>(static_cast<uint32_t>(ScanCode::KEY_A) + keyCode - 29);
        }
        if (keyCode >= 8 && keyCode <= 16) {
            return static_cast<ScanCode>(static_cast<uint32_t>(ScanCode::KEY_1) + keyCode - 8);
        }
        if (keyCode >= 131 && keyCode <= 142) {
            return static_cast<ScanCode>(static_cast<uint32_t>(ScanCode::KEY_F1) + keyCode - 131);
        }
        if (keyCode >= 145 && keyCode <= 153) {
            return static_cast<ScanCode>(static_cast<uint32_t>(ScanCode::KEY_KP_1) + keyCode - 145);
        }
        switch (keyCode) {
        case 7: return ScanCode::KEY_0;
        case 4:
        case 111: return ScanCode::KEY_ESCAPE;
        case 19: return ScanCode::KEY_UP;
        case 20: return ScanCode::KEY_DOWN;
        case 21: return ScanCode::KEY_LEFT;
        case 22: return ScanCode::KEY_RIGHT;
        case 23:
        case 66: return ScanCode::KEY_RETURN;
        case 55: return ScanCode::KEY_COMMA;
        case 56: return ScanCode::KEY_PERIOD;
        case 61: return ScanCode::KEY_TAB;
        case 62: return ScanCode::KEY_SPACE;
        case 67: return ScanCode::KEY_BACKSPACE;
        case 68: return ScanCode::KEY_GRAVE;
        case 69: return ScanCode::KEY_MINUS;
        case 70: return ScanCode::KEY_EQUALS;
        case 71: return ScanCode::KEY_LEFTBRACKET;
        case 72: return ScanCode::KEY_RIGHTBRACKET;
        case 73: return ScanCode::KEY_BACKSLASH;
        case 74: return ScanCode::KEY_SEMICOLON;
        case 75: return ScanCode::KEY_APOSTROPHE;
        case 76: return ScanCode::KEY_SLASH;
        case 92: return ScanCode::KEY_PAGEUP;
        case 93: return ScanCode::KEY_PAGEDOWN;
        case 112: return ScanCode::KEY_DELETE;
        case 115: return ScanCode::KEY_CAPSLOCK;
        case 116: return ScanCode::KEY_SCROLLLOCK;
        case 120: return ScanCode::KEY_PRINTSCREEN;
        case 121: return ScanCode::KEY_PAUSE;
        case 122: return ScanCode::KEY_HOME;
        case 123: return ScanCode::KEY_END;
        case 124: return ScanCode::KEY_INSERT;
        case 143: return ScanCode::KEY_NUMLOCKCLEAR;
        case 144: return ScanCode::KEY_KP_0;
        case 154: return ScanCode::KEY_KP_DIVIDE;
        case 155: return ScanCode::KEY_KP_MULTIPLY;
        case 156: return ScanCode::KEY_KP_MINUS;
        case 157: return ScanCode::KEY_KP_PLUS;
        case 158: return ScanCode::KEY_KP_PERIOD;
        case 160: return ScanCode::KEY_KP_ENTER;
        default: return ScanCode::KEY_NUM;
        }
    }

    KeyModFlags AndroidMetaStateToModifiers(int32_t metaState)
    {
        KeyModFlags result;
        const auto  applyPair = [&result, metaState](int32_t generic, int32_t left, int32_t right, KeyMod leftFlag, KeyMod rightFlag) {
            if ((metaState & left) != 0 || ((metaState & generic) != 0 && (metaState & (left | right)) == 0)) {
                result.SetBit(leftFlag);
            }
            if ((metaState & right) != 0) {
                result.SetBit(rightFlag);
            }
        };
        applyPair(0x1, 0x40, 0x80, KeyMod::LEFT_SHIFT, KeyMod::RIGHT_SHIFT);
        applyPair(0x2, 0x10, 0x20, KeyMod::LEFT_ALT, KeyMod::RIGHT_ALT);
        applyPair(0x1000, 0x2000, 0x4000, KeyMod::LEFT_CTRL, KeyMod::RIGHT_CTRL);
        applyPair(0x10000, 0x20000, 0x40000, KeyMod::LEFT_GUI, KeyMod::RIGHT_GUI);
        if ((metaState & 0x100000) != 0) {
            result.SetBit(KeyMod::CAPS);
        }
        if ((metaState & 0x200000) != 0) {
            result.SetBit(KeyMod::NUM);
        }
        return result;
    }

    namespace {
        uint32_t DecodeUtf8(std::string_view text, size_t &offset)
        {
            const auto first = static_cast<uint8_t>(text[offset++]);
            if (first < 0x80) {
                return first;
            }
            const uint32_t count = first < 0xE0 ? 2U : first < 0xF0 ? 3U : 4U;
            if (first < 0xC0 || first >= 0xF5 || offset + count - 1 > text.size()) {
                return 0xFFFD;
            }
            uint32_t code = first & (0x7FU >> count);
            for (uint32_t index = 1; index < count; ++index) {
                const auto byte = static_cast<uint8_t>(text[offset]);
                if ((byte & 0xC0) != 0x80) {
                    return 0xFFFD;
                }
                ++offset;
                code = (code << 6U) | (byte & 0x3FU);
            }
            if ((count == 2 && code < 0x80 && code != 0) || (count == 3 && code < 0x800) || (count == 4 && code < 0x10000) || code > 0x10FFFF) {
                return 0xFFFD;
            }
            return code;
        }

        void AppendUtf8(std::string &text, uint32_t code)
        {
            if (code == 0) {
                return; // Engine text events are null-terminated strings.
            }
            if (code < 0x80) {
                text.push_back(static_cast<char>(code));
            } else if (code < 0x800) {
                text.push_back(static_cast<char>(0xC0U | (code >> 6U)));
                text.push_back(static_cast<char>(0x80U | (code & 0x3FU)));
            } else if (code < 0x10000) {
                text.push_back(static_cast<char>(0xE0U | (code >> 12U)));
                text.push_back(static_cast<char>(0x80U | ((code >> 6U) & 0x3FU)));
                text.push_back(static_cast<char>(0x80U | (code & 0x3FU)));
            } else {
                text.push_back(static_cast<char>(0xF0U | (code >> 18U)));
                text.push_back(static_cast<char>(0x80U | ((code >> 12U) & 0x3FU)));
                text.push_back(static_cast<char>(0x80U | ((code >> 6U) & 0x3FU)));
                text.push_back(static_cast<char>(0x80U | (code & 0x3FU)));
            }
        }

        std::string FromModifiedUtf8(std::string_view text)
        {
            std::string result;
            size_t      offset = 0;
            while (offset < text.size()) {
                auto code = DecodeUtf8(text, offset);
                if (code >= 0xD800 && code <= 0xDBFF) {
                    size_t         next = offset;
                    const uint32_t low  = next < text.size() ? DecodeUtf8(text, next) : 0;
                    if (low >= 0xDC00 && low <= 0xDFFF) {
                        code   = 0x10000U + ((code - 0xD800U) << 10U) + low - 0xDC00U;
                        offset = next;
                    } else {
                        code = 0xFFFD;
                    }
                } else if (code >= 0xDC00 && code <= 0xDFFF) {
                    code = 0xFFFD;
                }
                AppendUtf8(result, code);
            }
            return result;
        }
    } // namespace

    AndroidTextEdit AndroidTextState::Apply(std::string_view modifiedUtf8, bool composing)
    {
        AndroidTextEdit edit;
        if (composing) {
            return edit;
        }
        const std::string next   = FromModifiedUtf8(modifiedUtf8);
        size_t            prefix = 0;
        while (prefix < committed.size() && prefix < next.size() && committed[prefix] == next[prefix]) {
            ++prefix;
        }
        while (prefix < next.size() && prefix != 0 && (static_cast<uint8_t>(next[prefix]) & 0xC0U) == 0x80U) {
            --prefix;
        }
        size_t oldEnd = committed.size();
        size_t newEnd = next.size();
        while (oldEnd > prefix && newEnd > prefix && committed[oldEnd - 1] == next[newEnd - 1]) {
            --oldEnd;
            --newEnd;
        }
        while (newEnd < next.size() && (static_cast<uint8_t>(next[newEnd]) & 0xC0U) == 0x80U) {
            ++oldEnd;
            ++newEnd;
        }
        for (size_t index = prefix; index < oldEnd; ++index) {
            if ((static_cast<uint8_t>(committed[index]) & 0xC0U) != 0x80U) {
                ++edit.deleted;
            }
        }
        edit.inserted = next.substr(prefix, newEnd - prefix);
        committed     = next;
        return edit;
    }

    void AndroidTextState::Reset()
    {
        committed.clear();
    }

} // namespace sky
