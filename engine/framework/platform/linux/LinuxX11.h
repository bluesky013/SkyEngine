//
// Private X11/xcb helpers shared by the Linux platform and window backends.
//

#pragma once

#include <X11/Xlib.h>
#include <xcb/xcb.h>

namespace sky::x11 {

    Display *GetDisplay();
    void SetDisplay(Display *display);

    // Translates the Xlib display into its xcb connection. Resolved lazily via
    // dlopen so the build does not require the X11-xcb dev package.
    xcb_connection_t *GetXcbConnection(Display *display);

    void RequestExit();
    bool ConsumeExit();

} // namespace sky::x11
