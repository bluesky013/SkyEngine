//
// Created on 2026/09/19.
//

#pragma once

#include <cstdint>
#include <string>

namespace sky::ui {

    enum class UIEventResult : uint8_t {
        UNHANDLED = 0,
        HANDLED,
    };

    enum class UIPointerAction : uint8_t {
        MOVE = 0,
        DOWN,
        UP,
        WHEEL,
    };

    struct UIPointerEvent {
        UIPointerAction action = UIPointerAction::MOVE;
        uint32_t pointerId = 0;
        uint32_t button = 0;
        float x = 0.0f;
        float y = 0.0f;
        float wheelDelta = 0.0f;
    };

    enum class UIKeyAction : uint8_t {
        DOWN = 0,
        UP,
        REPEAT,
    };

    struct UIKeyEvent {
        uint32_t keyCode = 0;
        UIKeyAction action = UIKeyAction::DOWN;
        uint32_t modifiers = 0;
    };

    struct UITextInputEvent {
        std::string text;
    };

    // Per-frame raw input snapshot fed by the framework input layer.
    struct UIInputState {
        float pointerX = 0.0f;
        float pointerY = 0.0f;
        uint32_t pointerButtons = 0;
        float wheelDelta = 0.0f;
    };

} // namespace sky::ui
