//
// Created on 2026/10/07.
//

#pragma once

#include <cstdint>

namespace sky::editor {

    // Modifier bits carried in sky::ui::UIKeyEvent::modifiers. Mirrors sky::KeyMod
    // (framework/window/IWindowEvent.h); the host forwards the platform flags
    // unchanged, so these are the values the UI layer tests against.
    inline constexpr uint32_t kModShift = 0x0003; // LEFT_SHIFT | RIGHT_SHIFT
    inline constexpr uint32_t kModCtrl  = 0x00C0; // LEFT_CTRL | RIGHT_CTRL

} // namespace sky::editor
