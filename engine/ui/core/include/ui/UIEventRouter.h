//
// Created on 2026/09/19.
//

#pragma once

#include <ui/UIElement.h>

#include <cstdint>
#include <unordered_map>

namespace sky::ui {

    class UIElement;
    class UIContext;

    class UIEventRouter {
    public:
        explicit UIEventRouter(UIContext &context);
        ~UIEventRouter();

        UIEventRouter(const UIEventRouter &)            = delete;
        UIEventRouter &operator=(const UIEventRouter &) = delete;

        // Topmost visible element under the point, in reverse paint order and
        // clipped by every ancestor that clips its children.
        UIElement *HitTest(float x, float y) const;

        UIEventResult DispatchPointer(const UIPointerEvent &event);
        UIEventResult DispatchKey(const UIKeyEvent &event);
        UIEventResult DispatchText(const UITextInputEvent &event);

        // Only elements currently attached to this context may receive focus.
        void       SetFocus(UIElement *element);
        UIElement *GetFocus() const;

        // Silent host reset. Tree mutations also clear affected state directly.
        void Reset();

        UIElement *GetCapture(uint32_t pointerId = 0) const;

        // Element currently under the pointer (nullptr if none).
        UIElement *GetHovered() const;

    private:
        friend class UIContext;
        struct DispatchGuard;
        void       CollectDetachedInput(UIElement *subtree, std::vector<UIElement::InputCancellation> &pending);
        UIElement *Resolve(const UIElement::InputReference &reference) const;
        static UIElement *
        HitTestElement(UIElement *element, float x, float y, const struct UIRect &clip, const struct UI2DTransform &parentTransform);

        bool UpdateHover(UIElement *target, const UIPointerEvent &event, const DispatchGuard &guard);
        void SetFocusFrom(UIElement *target);

        UIContext                *context;
        std::weak_ptr<void>       contextLifetime;
        std::shared_ptr<void>     inputLifetime      = std::make_shared<char>();
        uint64_t                  dispatchGeneration = 0;
        UIElement::InputReference focus;
        UIElement::InputReference hovered;
        // Capture is per pointer id so simultaneous pointers drag independently.
        std::unordered_map<uint32_t, UIElement::InputReference> captures;
    };

} // namespace sky::ui
