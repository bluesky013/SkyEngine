//
// Created on 2026/09/19.
//

#include <ui/UIEventRouter.h>
#include <ui/UIContext.h>
#include <ui/UIElement.h>

#include <functional>

namespace sky::ui {

    namespace {

        // Walks target -> ancestors, stopping at the first handler.
        UIEventResult Bubble(UIElement *target, const std::function<UIEventResult(UIElement *)> &handler)
        {
            UIElement *element = target;
            while (element != nullptr) {
                if (handler(element) == UIEventResult::HANDLED) {
                    return UIEventResult::HANDLED;
                }
                element = element->GetParent();
            }
            return UIEventResult::UNHANDLED;
        }

    } // namespace

    UIEventRouter::UIEventRouter(UIContext &context)
        : context(context)
    {
    }

    UIElement *UIEventRouter::HitTestElement(UIElement *element, float x, float y, const UIRect &clip)
    {
        if (element == nullptr || !element->IsEffectivelyVisible()) {
            return nullptr;
        }

        UIRect localClip = clip;
        if (element->ClipsChildren()) {
            localClip = UIRect::Intersect(clip, element->GetBounds());
        }

        const std::vector<UIElement *> order = element->GetPaintOrder();
        for (auto it = order.rbegin(); it != order.rend(); ++it) {
            if (UIElement *hit = HitTestElement(*it, x, y, localClip)) {
                return hit;
            }
        }

        if (element->GetBounds().Contains(x, y) && localClip.Contains(x, y)) {
            return element;
        }
        return nullptr;
    }

    UIElement *UIEventRouter::HitTest(float x, float y) const
    {
        UIElement *root = context.GetRoot();
        if (root == nullptr) {
            return nullptr;
        }
        return HitTestElement(root, x, y, root->GetBounds());
    }

    void UIEventRouter::UpdateHover(UIElement *target, const UIPointerEvent &event)
    {
        if (target == hovered) {
            return;
        }
        if (hovered != nullptr) {
            hovered->OnPointerLeave(event);
        }
        hovered = target;
        if (hovered != nullptr) {
            hovered->OnPointerEnter(event);
        }
    }

    void UIEventRouter::SetFocusFrom(UIElement *target)
    {
        UIElement *focusable = target;
        while (focusable != nullptr && !focusable->IsFocusable()) {
            focusable = focusable->GetParent();
        }
        focus = focusable;
    }

    UIElement *UIEventRouter::GetCapture(uint32_t pointerId) const
    {
        const auto it = captures.find(pointerId);
        return it != captures.end() ? it->second : nullptr;
    }

    UIEventResult UIEventRouter::DispatchPointer(const UIPointerEvent &event)
    {
        UIElement *activeCapture = GetCapture(event.pointerId);

        if (event.action == UIPointerAction::DOWN) {
            UIElement *hit = HitTest(event.x, event.y);
            SetFocusFrom(hit);
            if (hit == nullptr) {
                return UIEventResult::UNHANDLED;
            }
            captures[event.pointerId] = hit;
            return Bubble(hit, [&event](UIElement *element) { return element->OnPointerEvent(event); });
        }

        if (event.action == UIPointerAction::UP) {
            UIElement *target = activeCapture != nullptr ? activeCapture : HitTest(event.x, event.y);
            captures.erase(event.pointerId);
            if (target == nullptr) {
                return UIEventResult::UNHANDLED;
            }
            return Bubble(target, [&event](UIElement *element) { return element->OnPointerEvent(event); });
        }

        UIElement *target = activeCapture != nullptr ? activeCapture : HitTest(event.x, event.y);
        if (activeCapture == nullptr && event.pointerId == 0) {
            UpdateHover(target, event);
        }
        if (target == nullptr) {
            return UIEventResult::UNHANDLED;
        }
        return Bubble(target, [&event](UIElement *element) { return element->OnPointerEvent(event); });
    }

    UIEventResult UIEventRouter::DispatchKey(const UIKeyEvent &event)
    {
        if (focus == nullptr) {
            return UIEventResult::UNHANDLED;
        }
        return Bubble(focus, [&event](UIElement *element) { return element->OnKeyEvent(event); });
    }

    UIEventResult UIEventRouter::DispatchText(const UITextInputEvent &event)
    {
        if (focus == nullptr) {
            return UIEventResult::UNHANDLED;
        }
        return Bubble(focus, [&event](UIElement *element) { return element->OnTextInput(event); });
    }

} // namespace sky::ui
