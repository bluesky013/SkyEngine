//
// Overlay implementation (see Overlay.h).
//

#include <ui/widgets/Overlay.h>

#include <ui/detail/ChildRouting.h>

#include <ui/UIPaintContext.h>

#include <vector>

namespace sky::ui {

    void Overlay::OnPaint(UIPaintContext &context)
    {
        (void)context;
        const UIRect bounds = GetBounds();
        for (const auto &child : GetChildren()) {
            if (child) {
                child->SetBounds(bounds);
            }
        }
    }

    UIEventResult Overlay::RoutePointer(const UIPointerEvent &event)
    {
        return RouteChildPointer(GetChildren(), captured, event, /*topmostFirst=*/true);
    }

} // namespace sky::ui
