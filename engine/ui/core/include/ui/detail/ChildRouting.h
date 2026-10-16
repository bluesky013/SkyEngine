//
// Shared child pointer routing for layout containers (see StackPanel/DockPanel/GridPanel/Overlay).
//

#pragma once

#include <ui/UIElement.h>
#include <ui/UIEvent.h>

#include <vector>

namespace sky::ui {

    // `event` is in the parent's untransformed layout space, as received from
    // UIEventRouter. Maps it into each child's layout space, follows reverse
    // paint order, and checks ancestor clips unless captured (or a popup requests
    // capture). Clears `captured` before dispatching UP. `capturedPointerId`,
    // when supplied, records the DOWN that established this direct capture and
    // keeps other pointer ids from consuming or replacing it.
    // `topmostFirst` is a legacy ordering flag;
    // every container now follows z order, stable among equal-z siblings.
    UIEventResult RouteChildPointer(const std::vector<UIElementPtr> &children,
                                    UIElement                      *&captured,
                                    const UIPointerEvent            &event,
                                    bool                             topmostFirst,
                                    uint32_t                        *capturedPointerId = nullptr);

    // Releases the parent's direct capture before notifying the child.
    void CancelChildPointer(UIElement *&captured, uint32_t pointerId);

} // namespace sky::ui
