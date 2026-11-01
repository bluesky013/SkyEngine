#pragma once

#include <ace/xcomponent/native_interface_xcomponent.h>
#include <framework/window/IWindowEvent.h>

#include <cmath>
#include <limits>
#include <utility>

namespace sky {
    inline bool OHOSPixelCoordinate(float value, int32_t &output)
    {
        const double rounded = std::round(static_cast<double>(value));
        if (!std::isfinite(rounded) || rounded < std::numeric_limits<int32_t>::min() || rounded > std::numeric_limits<int32_t>::max()) {
            return false;
        }
        output = static_cast<int32_t>(rounded);
        return true;
    }

    inline ScanCode OHOSKeyCodeToScanCode(OH_NativeXComponent_KeyCode code)
    {
        static constexpr std::pair<OH_NativeXComponent_KeyCode, ScanCode> mappings[] = {
            {::KEY_A, ScanCode::KEY_A},
            {::KEY_B, ScanCode::KEY_B},
            {::KEY_C, ScanCode::KEY_C},
            {::KEY_D, ScanCode::KEY_D},
            {::KEY_E, ScanCode::KEY_E},
            {::KEY_F, ScanCode::KEY_F},
            {::KEY_G, ScanCode::KEY_G},
            {::KEY_H, ScanCode::KEY_H},
            {::KEY_I, ScanCode::KEY_I},
            {::KEY_J, ScanCode::KEY_J},
            {::KEY_K, ScanCode::KEY_K},
            {::KEY_L, ScanCode::KEY_L},
            {::KEY_M, ScanCode::KEY_M},
            {::KEY_N, ScanCode::KEY_N},
            {::KEY_O, ScanCode::KEY_O},
            {::KEY_P, ScanCode::KEY_P},
            {::KEY_Q, ScanCode::KEY_Q},
            {::KEY_R, ScanCode::KEY_R},
            {::KEY_S, ScanCode::KEY_S},
            {::KEY_T, ScanCode::KEY_T},
            {::KEY_U, ScanCode::KEY_U},
            {::KEY_V, ScanCode::KEY_V},
            {::KEY_W, ScanCode::KEY_W},
            {::KEY_X, ScanCode::KEY_X},
            {::KEY_Y, ScanCode::KEY_Y},
            {::KEY_Z, ScanCode::KEY_Z},
            {::KEY_0, ScanCode::KEY_0},
            {::KEY_1, ScanCode::KEY_1},
            {::KEY_2, ScanCode::KEY_2},
            {::KEY_3, ScanCode::KEY_3},
            {::KEY_4, ScanCode::KEY_4},
            {::KEY_5, ScanCode::KEY_5},
            {::KEY_6, ScanCode::KEY_6},
            {::KEY_7, ScanCode::KEY_7},
            {::KEY_8, ScanCode::KEY_8},
            {::KEY_9, ScanCode::KEY_9},
            {::KEY_F1, ScanCode::KEY_F1},
            {::KEY_F2, ScanCode::KEY_F2},
            {::KEY_F3, ScanCode::KEY_F3},
            {::KEY_F4, ScanCode::KEY_F4},
            {::KEY_F5, ScanCode::KEY_F5},
            {::KEY_F6, ScanCode::KEY_F6},
            {::KEY_F7, ScanCode::KEY_F7},
            {::KEY_F8, ScanCode::KEY_F8},
            {::KEY_F9, ScanCode::KEY_F9},
            {::KEY_F10, ScanCode::KEY_F10},
            {::KEY_F11, ScanCode::KEY_F11},
            {::KEY_F12, ScanCode::KEY_F12},
            {::KEY_NUMPAD_0, ScanCode::KEY_KP_0},
            {::KEY_NUMPAD_1, ScanCode::KEY_KP_1},
            {::KEY_NUMPAD_2, ScanCode::KEY_KP_2},
            {::KEY_NUMPAD_3, ScanCode::KEY_KP_3},
            {::KEY_NUMPAD_4, ScanCode::KEY_KP_4},
            {::KEY_NUMPAD_5, ScanCode::KEY_KP_5},
            {::KEY_NUMPAD_6, ScanCode::KEY_KP_6},
            {::KEY_NUMPAD_7, ScanCode::KEY_KP_7},
            {::KEY_NUMPAD_8, ScanCode::KEY_KP_8},
            {::KEY_NUMPAD_9, ScanCode::KEY_KP_9},
            {::KEY_ENTER, ScanCode::KEY_RETURN},
            {::KEY_ESCAPE, ScanCode::KEY_ESCAPE},
            {::KEY_DEL, ScanCode::KEY_BACKSPACE},
            {::KEY_FORWARD_DEL, ScanCode::KEY_DELETE},
            {::KEY_TAB, ScanCode::KEY_TAB},
            {::KEY_SPACE, ScanCode::KEY_SPACE},
            {::KEY_MINUS, ScanCode::KEY_MINUS},
            {::KEY_EQUALS, ScanCode::KEY_EQUALS},
            {::KEY_LEFT_BRACKET, ScanCode::KEY_LEFTBRACKET},
            {::KEY_RIGHT_BRACKET, ScanCode::KEY_RIGHTBRACKET},
            {::KEY_BACKSLASH, ScanCode::KEY_BACKSLASH},
            {::KEY_SEMICOLON, ScanCode::KEY_SEMICOLON},
            {::KEY_APOSTROPHE, ScanCode::KEY_APOSTROPHE},
            {::KEY_GRAVE, ScanCode::KEY_GRAVE},
            {::KEY_COMMA, ScanCode::KEY_COMMA},
            {::KEY_PERIOD, ScanCode::KEY_PERIOD},
            {::KEY_SLASH, ScanCode::KEY_SLASH},
            {::KEY_CAPS_LOCK, ScanCode::KEY_CAPSLOCK},
            {::KEY_SYSRQ, ScanCode::KEY_PRINTSCREEN},
            {::KEY_SCROLL_LOCK, ScanCode::KEY_SCROLLLOCK},
            {::KEY_BREAK, ScanCode::KEY_PAUSE},
            {::KEY_INSERT, ScanCode::KEY_INSERT},
            {::KEY_MOVE_HOME, ScanCode::KEY_HOME},
            {::KEY_PAGE_UP, ScanCode::KEY_PAGEUP},
            {::KEY_MOVE_END, ScanCode::KEY_END},
            {::KEY_PAGE_DOWN, ScanCode::KEY_PAGEDOWN},
            {::KEY_DPAD_RIGHT, ScanCode::KEY_RIGHT},
            {::KEY_DPAD_LEFT, ScanCode::KEY_LEFT},
            {::KEY_DPAD_DOWN, ScanCode::KEY_DOWN},
            {::KEY_DPAD_UP, ScanCode::KEY_UP},
            {::KEY_NUM_LOCK, ScanCode::KEY_NUMLOCKCLEAR},
            {::KEY_NUMPAD_DIVIDE, ScanCode::KEY_KP_DIVIDE},
            {::KEY_NUMPAD_MULTIPLY, ScanCode::KEY_KP_MULTIPLY},
            {::KEY_NUMPAD_SUBTRACT, ScanCode::KEY_KP_MINUS},
            {::KEY_NUMPAD_ADD, ScanCode::KEY_KP_PLUS},
            {::KEY_NUMPAD_ENTER, ScanCode::KEY_KP_ENTER},
            {::KEY_NUMPAD_DOT, ScanCode::KEY_KP_PERIOD},
        };
        for (const auto &entry : mappings) {
            if (entry.first == code) {
                return entry.second;
            }
        }
        return ScanCode::KEY_NUM;
    }

    inline KeyModFlags OHOSKeyModifiers(uint64_t native, bool num, bool caps)
    {
        KeyModFlags result;
        if ((native & ARKUI_MODIFIER_KEY_SHIFT) != 0)
            result |= KeyMod::SHIFT;
        if ((native & ARKUI_MODIFIER_KEY_CTRL) != 0)
            result |= KeyMod::CTRL;
        if ((native & ARKUI_MODIFIER_KEY_ALT) != 0)
            result |= KeyMod::ALT;
        if (num)
            result |= KeyMod::NUM;
        if (caps)
            result |= KeyMod::CAPS;
        return result;
    }
} // namespace sky
