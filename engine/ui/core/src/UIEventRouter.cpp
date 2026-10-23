//
// Created on 2026/09/19.
//

#include <ui/UIContext.h>
#include <ui/UIEventRouter.h>

#include <algorithm>
#include <functional>

namespace sky::ui {

    namespace {
        using InputReference = UIElement::InputReference;

        InputReference Reference(UIElement *element)
        {
            return element != nullptr ? element->GetInputReference() : InputReference{};
        }

        // Snapshot each attachment before entering user code. A callback may
        // destroy a handler, its ancestors, the router, or the whole context.
        UIEventResult Bubble(UIElement *target, const std::function<UIEventResult(UIElement *)> &handler, const std::function<bool()> &isCurrent)
        {
            auto reference = Reference(target);
            while (UIElement *element = reference.Get()) {
                const auto          parent = Reference(element->GetParent());
                const UIEventResult result = handler(element);
                if (result == UIEventResult::HANDLED) {
                    return result;
                }
                if (!isCurrent() || reference.Get() == nullptr) {
                    return UIEventResult::UNHANDLED;
                }
                reference = parent;
            }
            return UIEventResult::UNHANDLED;
        }

        bool MapPointer(UIElement &element, const UIPointerEvent &surfaceEvent, UIPointerEvent &mapped)
        {
            UIPoint point;
            if (!element.GetGeometry().SurfaceToLayout(surfaceEvent.x, surfaceEvent.y, point)) {
                return false;
            }
            mapped   = surfaceEvent;
            mapped.x = point.x;
            mapped.y = point.y;
            return true;
        }

        UIEventResult BubblePointer(UIElement *target, const UIPointerEvent &event, const std::function<bool()> &isCurrent)
        {
            return Bubble(
                target,
                [&event](UIElement *element) {
                    UIPointerEvent mapped;
                    return MapPointer(*element, event, mapped) ? element->OnPointerEvent(mapped) : UIEventResult::UNHANDLED;
                },
                isCurrent);
        }

        void CancelPointer(UIElement *target, uint32_t pointerId, const std::function<bool()> &isCurrent)
        {
            std::vector<InputReference> path;
            for (UIElement *element = target; element != nullptr; element = element->GetParent()) {
                path.push_back(element->GetInputReference());
            }
            for (const auto &reference : path) {
                if (!isCurrent()) {
                    return;
                }
                if (auto *element = reference.Get()) {
                    element->OnPointerCancel(pointerId);
                    if (reference.Get() == nullptr) {
                        return;
                    }
                }
            }
        }
    } // namespace

    struct UIEventRouter::DispatchGuard {
        UIEventRouter      *router;
        std::weak_ptr<void> routerLifetime;
        std::weak_ptr<void> contextLifetime;
        uint64_t            generation;

        bool IsCurrent() const
        {
            return !routerLifetime.expired() && !contextLifetime.expired() && router->dispatchGeneration == generation;
        }
    };

    UIEventRouter::UIEventRouter(UIContext &value) : context(&value), contextLifetime(value.inputLifetime)
    {
        value.inputRouters.push_back({this, inputLifetime});
    }

    UIEventRouter::~UIEventRouter()
    {
        inputLifetime.reset();
        if (!contextLifetime.expired()) {
            auto &entries = context->inputRouters;
            entries.erase(std::remove_if(entries.begin(), entries.end(), [this](const auto &entry) { return entry.router == this; }), entries.end());
        }
    }

    UIElement *UIEventRouter::Resolve(const InputReference &reference) const
    {
        UIElement *element = reference.Get();
        return !contextLifetime.expired() && element != nullptr && element->GetInputContext() == context ? element : nullptr;
    }

    void UIEventRouter::SetFocus(UIElement *element)
    {
        if (element == nullptr) {
            focus = {};
        } else if (!contextLifetime.expired() && element->GetInputContext() == context) {
            focus = element->GetInputReference();
        }
    }

    UIElement *UIEventRouter::GetFocus() const
    {
        return Resolve(focus);
    }

    UIElement *UIEventRouter::GetHovered() const
    {
        return Resolve(hovered);
    }

    void UIEventRouter::CollectDetachedInput(UIElement *subtree, std::vector<UIElement::InputCancellation> &pending)
    {
        auto detached = [subtree](const InputReference &reference) {
            for (UIElement *element = reference.GetAlive(); element != nullptr; element = element->GetParent()) {
                if (element == subtree) {
                    return true;
                }
            }
            return false;
        };
        bool changed = false;
        auto collect = [&](InputReference &reference, uint32_t pointerId) {
            if (detached(reference)) {
                pending.push_back({reference.GetAlive()->GetInputReference(), pointerId});
                reference = {};
                changed   = true;
            }
        };
        collect(focus, 0);
        collect(hovered, 0);
        for (auto it = captures.begin(); it != captures.end();) {
            if (detached(it->second.target)) {
                pending.push_back({it->second.target.GetAlive()->GetInputReference(), it->first});
                it      = captures.erase(it);
                changed = true;
            } else {
                ++it;
            }
        }
        if (changed) {
            ++dispatchGeneration;
        }
    }

    UIElement *UIEventRouter::HitTestElement(UIElement *element, float x, float y, const UIRect &clip, const UI2DTransform &parentTransform)
    {
        if (element == nullptr || !element->IsVisible() || !clip.Contains(x, y)) {
            return nullptr;
        }
        const UIGeometry geometry{element->GetBounds(), parentTransform * element->GetTransform().ToMatrix()};
        UIPoint          point;
        if (!geometry.SurfaceToLayout(x, y, point)) {
            return nullptr;
        }
        UIRect localClip = clip;
        if (element->ClipsChildren()) {
            localClip = UIRect::Intersect(clip, geometry.SurfaceBounds());
        }
        const auto order = element->GetPaintOrder();
        for (auto it = order.rbegin(); it != order.rend(); ++it) {
            if (UIElement *hit = HitTestElement(*it, x, y, localClip, geometry.layoutToSurface)) {
                return hit;
            }
        }
        return element->GetBounds().Contains(point.x, point.y) && localClip.Contains(x, y) ? element : nullptr;
    }

    UIElement *UIEventRouter::HitTest(float x, float y) const
    {
        if (contextLifetime.expired()) {
            return nullptr;
        }
        UIElement *root = context->GetRoot();
        return root != nullptr ? HitTestElement(root, x, y, root->GetBounds(), context->GetSurfaceTransform()) : nullptr;
    }

    bool UIEventRouter::UpdateHover(UIElement *target, const UIPointerEvent &event, const DispatchGuard &guard)
    {
        if (target == GetHovered()) {
            return true;
        }
        const auto next     = Reference(target);
        UIElement *previous = GetHovered();
        hovered             = {};
        if (previous != nullptr) {
            UIPointerEvent mapped;
            if (MapPointer(*previous, event, mapped)) {
                previous->OnPointerLeave(mapped);
            } else {
                previous->OnPointerCancel(event.pointerId);
            }
            if (!guard.IsCurrent()) {
                return false;
            }
        }
        target  = Resolve(next);
        hovered = Reference(target);
        if (target != nullptr) {
            UIPointerEvent mapped;
            if (MapPointer(*target, event, mapped)) {
                target->OnPointerEnter(mapped);
                if (!guard.IsCurrent()) {
                    return false;
                }
            }
        }
        return true;
    }

    void UIEventRouter::SetFocusFrom(UIElement *target)
    {
        while (target != nullptr && !target->IsFocusable()) {
            target = target->GetParent();
        }
        focus = Reference(target);
    }

    UIElement *UIEventRouter::GetCapture(uint32_t pointerId) const
    {
        const auto it = captures.find(pointerId);
        return it != captures.end() ? Resolve(it->second.target) : nullptr;
    }

    UIEventResult UIEventRouter::DispatchPointer(const UIPointerEvent &event)
    {
        if (contextLifetime.expired()) {
            return UIEventResult::UNHANDLED;
        }
        const DispatchGuard guard{this, inputLifetime, contextLifetime, ++dispatchGeneration};
        const auto          isCurrent     = [&guard] { return guard.IsCurrent(); };
        UIElement          *activeCapture = GetCapture(event.pointerId);
        if (activeCapture != nullptr) {
            UIPointerEvent mapped;
            if (!MapPointer(*activeCapture, event, mapped)) {
                captures.erase(event.pointerId);
                CancelPointer(activeCapture, event.pointerId, isCurrent);
                return UIEventResult::UNHANDLED;
            }
            if ((event.action == UIPointerAction::DOWN || event.action == UIPointerAction::UP) &&
                event.button != captures.find(event.pointerId)->second.button) {
                if (event.action == UIPointerAction::DOWN)
                    activeCapture->OnPointerButtonConflict(event.pointerId);
                return UIEventResult::HANDLED;
            }
        }
        if (event.action == UIPointerAction::DOWN && activeCapture == nullptr) {
            UIElement *hit = HitTest(event.x, event.y);
            SetFocusFrom(hit);
            if (hit == nullptr) {
                return UIEventResult::UNHANDLED;
            }
            captures[event.pointerId] = {hit->GetInputReference(), event.button};
            return BubblePointer(hit, event, isCurrent);
        }
        UIElement *target    = activeCapture != nullptr ? activeCapture : HitTest(event.x, event.y);
        const auto reference = Reference(target);
        if (event.action == UIPointerAction::UP) {
            captures.erase(event.pointerId);
        } else if (activeCapture == nullptr && event.pointerId == 0) {
            if (!UpdateHover(target, event, guard)) {
                return UIEventResult::UNHANDLED;
            }
        }
        target = reference.Get();
        return target != nullptr ? BubblePointer(target, event, isCurrent) : UIEventResult::UNHANDLED;
    }

    UIEventResult UIEventRouter::DispatchKey(const UIKeyEvent &event)
    {
        const DispatchGuard guard{this, inputLifetime, contextLifetime, ++dispatchGeneration};
        return Bubble(GetFocus(), [&event](UIElement *element) { return element->OnKeyEvent(event); }, [&guard] { return guard.IsCurrent(); });
    }

    UIEventResult UIEventRouter::DispatchText(const UITextInputEvent &event)
    {
        const DispatchGuard guard{this, inputLifetime, contextLifetime, ++dispatchGeneration};
        return Bubble(GetFocus(), [&event](UIElement *element) { return element->OnTextInput(event); }, [&guard] { return guard.IsCurrent(); });
    }

    void UIEventRouter::Reset()
    {
        ++dispatchGeneration;
        focus   = {};
        hovered = {};
        captures.clear();
    }
} // namespace sky::ui
