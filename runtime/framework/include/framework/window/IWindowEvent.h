//
// Created by Zach Lee on 2022/1/3.
//

#pragma once

#include <core/event/Event.h>
#include <core/template/Flags.h>

namespace sky {
    class NativeWindow;

#if _WIN32
    using WindowID = uint32_t;
#else
    using WindowID = uint64_t;
#endif
    static const WindowID INVALID_WIN_ID = 0;

    enum class MouseButtonType : uint32_t {
        LEFT,
        RIGHT,
        MIDDLE
    };

    struct WindowResizeEvent {
        WindowID winID;
        uint32_t width;
        uint32_t height;
    };

    struct MouseButtonEvent {
        WindowID winID;
        MouseButtonType button;
        uint32_t clicks;
        int32_t x;
        int32_t y;
    };

    struct MouseMotionEvent {
        WindowID winID;
        int32_t x;
        int32_t y;
        int32_t relX;
        int32_t relY;
    };

    struct MouseWheelEvent {
        WindowID winID;
        int32_t x;
        int32_t y;
    };

    class IMouseEvent : public EventTraits {
    public:
        IMouseEvent() = default;
        virtual ~IMouseEvent() = default;

        virtual void OnMouseButtonDown(const MouseButtonEvent &event) {}
        virtual void OnMouseButtonUp(const MouseButtonEvent &event) {}
        virtual void OnMouseMotion(const MouseMotionEvent &event) {}
        virtual void OnMouseWheel(const MouseWheelEvent &event) {}
    };

    class IDropEvent : public EventTraits {
    public:
        IDropEvent() = default;
        virtual ~IDropEvent() = default;

        virtual void OnDrop(const std::string& payload) {}
    };

    enum class ScanCode : uint32_t {
        KEY_A = 4,
        KEY_B = 5,
        KEY_C = 6,
        KEY_D = 7,
        KEY_E = 8,
        KEY_F = 9,
        KEY_G = 10,
        KEY_H = 11,
        KEY_I = 12,
        KEY_J = 13,
        KEY_K = 14,
        KEY_L = 15,
        KEY_M = 16,
        KEY_N = 17,
        KEY_O = 18,
        KEY_P = 19,
        KEY_Q = 20,
        KEY_R = 21,
        KEY_S = 22,
        KEY_T = 23,
        KEY_U = 24,
        KEY_V = 25,
        KEY_W = 26,
        KEY_X = 27,
        KEY_Y = 28,
        KEY_Z = 29,
        KEY_1 = 30,
        KEY_2 = 31,
        KEY_3 = 32,
        KEY_4 = 33,
        KEY_5 = 34,
        KEY_6 = 35,
        KEY_7 = 36,
        KEY_8 = 37,
        KEY_9 = 38,
        KEY_0 = 39,
        KEY_RETURN = 40,
        KEY_ESCAPE = 41,
        KEY_BACKSPACE = 42,
        KEY_TAB = 43,
        KEY_SPACE = 44,
        KEY_MINUS = 45,
        KEY_EQUALS = 46,
        KEY_LEFTBRACKET = 47,
        KEY_RIGHTBRACKET = 48,
        KEY_BACKSLASH = 49,
        KEY_NONUSHASH = 50,
        KEY_SEMICOLON = 51,
        KEY_APOSTROPHE = 52,
        KEY_GRAVE = 53,
        KEY_COMMA = 54,
        KEY_PERIOD = 55,
        KEY_SLASH = 56,
        KEY_CAPSLOCK = 57,
        KEY_F1 = 58,
        KEY_F2 = 59,
        KEY_F3 = 60,
        KEY_F4 = 61,
        KEY_F5 = 62,
        KEY_F6 = 63,
        KEY_F7 = 64,
        KEY_F8 = 65,
        KEY_F9 = 66,
        KEY_F10 = 67,
        KEY_F11 = 68,
        KEY_F12 = 69,
        KEY_PRINTSCREEN = 70,
        KEY_SCROLLLOCK = 71,
        KEY_PAUSE = 72,
        KEY_INSERT = 73,
        KEY_HOME = 74,
        KEY_PAGEUP = 75,
        KEY_DELETE = 76,
        KEY_END = 77,
        KEY_PAGEDOWN = 78,
        KEY_RIGHT = 79,
        KEY_LEFT = 80,
        KEY_DOWN = 81,
        KEY_UP = 82,
        KEY_NUMLOCKCLEAR = 83,
        KEY_KP_DIVIDE = 84,
        KEY_KP_MULTIPLY = 85,
        KEY_KP_MINUS = 86,
        KEY_KP_PLUS = 87,
        KEY_KP_ENTER = 88,
        KEY_KP_1 = 89,
        KEY_KP_2 = 90,
        KEY_KP_3 = 91,
        KEY_KP_4 = 92,
        KEY_KP_5 = 93,
        KEY_KP_6 = 94,
        KEY_KP_7 = 95,
        KEY_KP_8 = 96,
        KEY_KP_9 = 97,
        KEY_KP_0 = 98,
        KEY_KP_PERIOD = 99,
        KEY_NUM
    };

    enum class KeyMod : uint32_t  {
        NONE        = 0x0000,
        LEFT_SHIFT  = 0x0001,
        RIGHT_SHIFT = 0x0002,
        LEFT_CTRL   = 0x0040,
        RIGHT_CTRL  = 0x0080,
        LEFT_ALT    = 0x0100,
        RIGHT_ALT   = 0x0200,
        LEFT_GUI    = 0x0400,
        RIGHT_GUI   = 0x0800,
        NUM         = 0x1000,
        CAPS        = 0x2000,

        SHIFT = LEFT_SHIFT | RIGHT_SHIFT,
        CTRL = LEFT_CTRL | RIGHT_CTRL,
        ALT = LEFT_ALT | RIGHT_ALT,
        GUI = LEFT_GUI | RIGHT_GUI
    };
    using KeyModFlags = Flags<KeyMod>;

    struct KeyboardEvent {
        WindowID winID;
        ScanCode scanCode;
        KeyModFlags mod;
    };

    class IKeyboardEvent : public EventTraits {
    public:
        virtual void OnKeyUp(const KeyboardEvent &event) {}
        virtual void OnKeyDown(const KeyboardEvent &event) {}
        virtual void OnTextInput(WindowID windID, const char *text) {}
    };

    class IWindowEvent : public EventTraits {
    public:
        using KeyType = const NativeWindow*;

        IWindowEvent()          = default;
        virtual ~IWindowEvent() = default;

        virtual void OnWindowResize(const WindowResizeEvent& event) {}
        virtual void OnFocusChanged(bool focus) {}
    };

} // namespace sky
