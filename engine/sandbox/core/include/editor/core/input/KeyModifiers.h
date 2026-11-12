//
// Created on 2026/10/07.
//

#pragma once

#include <cstdint>

namespace sky::editor {

    // The host forwards sky::KeyMod bits on key and pointer button events.
    inline constexpr uint32_t kModShift = 0x0003; // LEFT_SHIFT | RIGHT_SHIFT
    inline constexpr uint32_t kModCtrl  = 0x00C0; // LEFT_CTRL | RIGHT_CTRL
    inline constexpr uint32_t kModAlt   = 0x0300; // LEFT_ALT | RIGHT_ALT

} // namespace sky::editor
