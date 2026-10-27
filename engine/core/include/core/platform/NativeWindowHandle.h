//
// Created by bluesky on 2026/9/21.
//

#pragma once

#include <cstdint>

namespace sky {

    // Native window handle contract shared between the window layer (framework)
    // and graphics backends (aurora). X11/xcb needs both the connection and the
    // window, so it is passed as a pointer to this struct via SwapChain::Descriptor.window.
    struct XcbWindowHandle {
        void *connection = nullptr; // xcb_connection_t*
        void *window     = nullptr; // xcb_window_t
    };

} // namespace sky
