//
// Created by Zach Lee on 2022/1/3.
//


#pragma once

#include <framework/event/Event.h>

namespace sky {

    namespace MouseButton {
        static constexpr uint8_t MOUSE_BUTTON_LEFT   = 1;
        static constexpr uint8_t MOUSE_BUTTON_RIGHT  = 2;
        static constexpr uint8_t MOUSE_BUTTON_MIDDLE = 3;
    }
    using MouseButtonType = uint8_t;

    namespace KeyButton {
        static constexpr uint16_t KEY_A            = 1;
        static constexpr uint16_t KEY_B            = 2;
        static constexpr uint16_t KEY_C            = 3;
        static constexpr uint16_t KEY_D            = 4;
        static constexpr uint16_t KEY_E            = 5;
        static constexpr uint16_t KEY_F            = 6;
        static constexpr uint16_t KEY_G            = 7;
        static constexpr uint16_t KEY_H            = 8;
        static constexpr uint16_t KEY_I            = 9;
        static constexpr uint16_t KEY_J            = 10;
        static constexpr uint16_t KEY_K            = 11;
        static constexpr uint16_t KEY_L            = 12;
        static constexpr uint16_t KEY_M            = 13;
        static constexpr uint16_t KEY_N            = 14;
        static constexpr uint16_t KEY_O            = 15;
        static constexpr uint16_t KEY_P            = 16;
        static constexpr uint16_t KEY_Q            = 17;
        static constexpr uint16_t KEY_R            = 18;
        static constexpr uint16_t KEY_S            = 19;
        static constexpr uint16_t KEY_T            = 20;
        static constexpr uint16_t KEY_U            = 21;
        static constexpr uint16_t KEY_V            = 22;
        static constexpr uint16_t KEY_W            = 23;
        static constexpr uint16_t KEY_X            = 24;
        static constexpr uint16_t KEY_Y            = 25;
        static constexpr uint16_t KEY_Z            = 26;
        static constexpr uint16_t KEY_1            = 27;
        static constexpr uint16_t KEY_2            = 28;
        static constexpr uint16_t KEY_3            = 29;
        static constexpr uint16_t KEY_4            = 30;
        static constexpr uint16_t KEY_5            = 31;
        static constexpr uint16_t KEY_6            = 32;
        static constexpr uint16_t KEY_7            = 33;
        static constexpr uint16_t KEY_8            = 34;
        static constexpr uint16_t KEY_9            = 35;
        static constexpr uint16_t KEY_0            = 36;
        static constexpr uint16_t KEY_RETURN       = 37;
        static constexpr uint16_t KEY_ESCAPE       = 38;
        static constexpr uint16_t KEY_BACKSPACE    = 39;
        static constexpr uint16_t KEY_TAB          = 40;
        static constexpr uint16_t KEY_SPACE        = 41;
        static constexpr uint16_t KEY_MINUS        = 42;
        static constexpr uint16_t KEY_EQUALS       = 43;
        static constexpr uint16_t KEY_LEFTBRACKET  = 44;
        static constexpr uint16_t KEY_RIGHTBRACKET = 45;
        static constexpr uint16_t KEY_BACKSLASH    = 46;
        static constexpr uint16_t KEY_NONUSHASH    = 47;
        static constexpr uint16_t KEY_SEMICOLON    = 48;
        static constexpr uint16_t KEY_APOSTROPHE   = 49;
        static constexpr uint16_t KEY_GRAVE        = 50;
        static constexpr uint16_t KEY_COMMA        = 51;
        static constexpr uint16_t KEY_PERIOD       = 52;
        static constexpr uint16_t KEY_SLASH        = 53;
        static constexpr uint16_t KEY_CAPSLOCK     = 54;
        static constexpr uint16_t KEY_F1           = 55;
        static constexpr uint16_t KEY_F2           = 56;
        static constexpr uint16_t KEY_F3           = 57;
        static constexpr uint16_t KEY_F4           = 58;
        static constexpr uint16_t KEY_F5           = 59;
        static constexpr uint16_t KEY_F6           = 60;
        static constexpr uint16_t KEY_F7           = 61;
        static constexpr uint16_t KEY_F8           = 62;
        static constexpr uint16_t KEY_F9           = 63;
        static constexpr uint16_t KEY_F10          = 64;
        static constexpr uint16_t KEY_F11          = 65;
        static constexpr uint16_t KEY_F12          = 66;
        static constexpr uint16_t KEY_PRINTSCREEN  = 67;
        static constexpr uint16_t KEY_SCROLLLOCK   = 68;
        static constexpr uint16_t KEY_PAUSE        = 69;
        static constexpr uint16_t KEY_INSERT       = 70;
        static constexpr uint16_t KEY_HOME         = 71;
        static constexpr uint16_t KEY_PAGEUP       = 72;
        static constexpr uint16_t KEY_DELETE       = 73;
        static constexpr uint16_t KEY_END          = 74;
        static constexpr uint16_t KEY_PAGEDOWN     = 75;
        static constexpr uint16_t KEY_RIGHT        = 76;
        static constexpr uint16_t KEY_LEFT         = 77;
        static constexpr uint16_t KEY_DOWN         = 78;
        static constexpr uint16_t KEY_UP           = 79;
        static constexpr uint16_t KEY_NUMLOCKCLEAR = 80;
        static constexpr uint16_t KEY_KP_DIVIDE    = 81;
        static constexpr uint16_t KEY_KP_MULTIPLY  = 82;
        static constexpr uint16_t KEY_KP_MINUS     = 83;
        static constexpr uint16_t KEY_KP_PLUS      = 84;
        static constexpr uint16_t KEY_KP_ENTER     = 85;
        static constexpr uint16_t KEY_KP_1         = 86;
        static constexpr uint16_t KEY_KP_2         = 87;
        static constexpr uint16_t KEY_KP_3         = 88;
        static constexpr uint16_t KEY_KP_4         = 89;
        static constexpr uint16_t KEY_KP_5         = 90;
        static constexpr uint16_t KEY_KP_6         = 91;
        static constexpr uint16_t KEY_KP_7         = 92;
        static constexpr uint16_t KEY_KP_8         = 93;
        static constexpr uint16_t KEY_KP_9         = 94;
        static constexpr uint16_t KEY_KP_0         = 95;
        static constexpr uint16_t KEY_KP_PERIOD    = 96;
    }
    using KeyButtonType = uint16_t;

    class IWindowEvent : public EventTraits {
    public:
        using KeyType = void*;

        IWindowEvent() = default;
        virtual ~IWindowEvent() = default;

        virtual void OnWindowResize(uint32_t width, uint32_t height) {}

        // mouse
        virtual void OnMouseMove(int32_t x, int32_t y) {}
        virtual void OnMouseButtonDown(MouseButtonType button) {}
        virtual void OnMouseButtonUp(MouseButtonType button) {}
        virtual void OnMouseWheel(int32_t wheelX, int32_t wheelY) {}

        virtual void OnKeyUp(KeyButtonType) {}
        virtual void OnKeyDown(KeyButtonType) {}
        virtual void OnTextInput(const char* text) {}
    };

}