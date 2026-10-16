//
// ChildRouting implementation (see ChildRouting.h).
//

#include <ui/detail/ChildRouting.h>

#include <algorithm>

namespace sky::ui {

    namespace {

        bool MapChildPointer(const UIElement &child, const UIPointerEvent &parentEvent, UIPointerEvent &mapped)
        {
            // Individually invertible matrices can still overflow or underflow
            // when composed. Match the router's accumulated-path validity.
            UI2DTransform accumulatedInverse;
            if (!child.GetGeometry().layoutToSurface.TryInverse(accumulatedInverse)) {
                return false;
            }
            UIPoint point;
            if (!UIGeometry{child.GetBounds(), child.GetTransform().ToMatrix()}.SurfaceToLayout(parentEvent.x, parentEvent.y, point)) {
                return false;
            }
            mapped   = parentEvent;
            mapped.x = point.x;
            mapped.y = point.y;
            return true;
        }

        bool IsInsideParentClip(const UIElement *parent, const UIPointerEvent &event)
        {
            if (parent == nullptr) {
                return true;
            }
            const UIGeometry geometry = parent->GetGeometry();
            float            x        = event.x;
            float            y        = event.y;
            geometry.layoutToSurface.Apply(x, y);

            for (const UIElement *ancestor = parent; ancestor != nullptr; ancestor = ancestor->GetParent()) {
                if (ancestor->ClipsChildren() && !ancestor->GetGeometry().SurfaceBounds().Contains(x, y)) {
                    return false;
                }
            }
            // The host owns the viewport clip; a detached container need not
            // clip overflow to its own bounds when ClipsChildren is false.
            return true;
        }

    } // namespace

    void CancelChildPointer(UIElement *&captured, uint32_t pointerId)
    {
        UIElement *target = captured;
        captured          = nullptr;
        if (target != nullptr) {
            target->OnPointerCancel(pointerId);
        }
    }

    UIEventResult RouteChildPointer(
        const std::vector<UIElementPtr> &children, UIElement *&captured, const UIPointerEvent &event, bool topmostFirst, uint32_t *capturedPointerId)
    {
        // All containers now follow the same reverse paint order. Keep the old
        // argument so existing direct-routing callers retain source compatibility.
        (void)topmostFirst;
        if (captured != nullptr) {
            const auto found =
                std::find_if(children.begin(), children.end(), [captured](const UIElementPtr &child) { return child.get() == captured; });
            if (found == children.end()) {
                captured = nullptr;
                if (capturedPointerId != nullptr) {
                    *capturedPointerId = 0;
                }
            }
        }
        if (captured != nullptr && capturedPointerId != nullptr && *capturedPointerId != event.pointerId) {
            // These containers retain one direct capture at a time. Another
            // pointer cannot consume its release or replace its target.
            return UIEventResult::UNHANDLED;
        }
        const auto       firstChild = std::find_if(children.begin(), children.end(), [](const UIElementPtr &child) { return child != nullptr; });
        const UIElement *parent     = firstChild == children.end() ? nullptr : (*firstChild)->GetParent();
        const UIElement::InputReference parentReference = parent != nullptr ? parent->GetInputReference() : UIElement::InputReference{};
        const auto                      ownerIsCurrent = [&parentReference, parent] { return parent == nullptr || parentReference.Get() != nullptr; };
        const uint32_t                  activePointerId = capturedPointerId != nullptr ? *capturedPointerId : event.pointerId;
        if (parent != nullptr) {
            UI2DTransform inverse;
            if (!parent->GetGeometry().layoutToSurface.TryInverse(inverse)) {
                CancelChildPointer(captured, activePointerId);
                return UIEventResult::UNHANDLED;
            }
        }
        if (captured != nullptr) {
            UIPointerEvent mapped;
            if (!MapChildPointer(*captured, event, mapped)) {
                CancelChildPointer(captured, activePointerId);
                return UIEventResult::UNHANDLED;
            }
            UIElement *target = captured;
            if (event.action == UIPointerAction::UP) {
                captured = nullptr;
                if (capturedPointerId != nullptr) {
                    *capturedPointerId = 0;
                }
            }
            // A handler may remove this container. Never write through its
            // captured member, or access its child vector, after dispatch.
            return target->OnPointerEvent(mapped);
        }

        std::vector<UIElement::InputReference> order;
        order.reserve(children.size());
        for (const auto &child : children) {
            if (child != nullptr && child->IsVisible()) {
                order.push_back(child->GetInputReference());
            }
        }
        std::stable_sort(order.begin(), order.end(), [](const UIElement::InputReference &a, const UIElement::InputReference &b) {
            return a.Get()->GetLayout().z < b.Get()->GetLayout().z;
        });
        for (auto it = order.rbegin(); it != order.rend(); ++it) {
            UIElement *element = it->Get();
            if (element == nullptr || !element->IsVisible()) {
                continue;
            }
            // WantsPointerCapture is a user-overridable callback. It may
            // destroy/reparent the target, its siblings, or this container.
            const bool wantsCapture = element->WantsPointerCapture();
            if (!ownerIsCurrent()) {
                return UIEventResult::UNHANDLED;
            }
            element = it->Get();
            if (element == nullptr || !element->IsVisible()) {
                continue;
            }
            UIPointerEvent mapped;
            if (!MapChildPointer(*element, event, mapped)) {
                continue;
            }
            if (!wantsCapture && (!IsInsideParentClip(parent, event) || !element->GetBounds().Contains(mapped.x, mapped.y))) {
                continue;
            }
            if (event.action == UIPointerAction::DOWN) {
                captured = element;
                if (capturedPointerId != nullptr) {
                    *capturedPointerId = event.pointerId;
                }
            }
            return element->OnPointerEvent(mapped);
        }
        return UIEventResult::UNHANDLED;
    }

} // namespace sky::ui
