//
// Created on 2026/10/06.
//

#pragma once

#include <cstdint>

namespace sky {

    // Standard OS cursors a host may request (e.g. a resize cursor over a splitter).
    enum class StandardCursor : uint8_t {
        Arrow = 0,
        ResizeHorizontal,
        ResizeVertical,
        ResizeAll,
        Text,
        Hand,
    };

} // namespace sky
