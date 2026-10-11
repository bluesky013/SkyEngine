//
// Shared child pointer routing for layout containers (see StackPanel/DockPanel/GridPanel/Overlay).
//

#pragma once

#include <ui/UIElement.h>
#include <ui/UIEvent.h>

#include <vector>

namespace sky::ui {

    // Delivers `event` to the child under the cursor (or to `captured` while a press is active),
    // clearing `captured` on UP. Returns UNHANDLED when no child is hit. `topmostFirst` reverses
    // the paint order (for overlays). Containers keep their own slot bookkeeping; only the routing
    // is shared.
    UIEventResult RouteChildPointer(const std::vector<UIElementPtr> &children, UIElement *&captured, const UIPointerEvent &event, bool topmostFirst);

} // namespace sky::ui
