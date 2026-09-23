//
// Created on 2026/09/19.
//

#pragma once

#include <ui/UIEvent.h>

#include <cstdint>
#include <unordered_map>

namespace sky::ui {

    class UIElement;
    class UIContext;

    class UIEventRouter {
    public:
        explicit UIEventRouter(UIContext &context);
        ~UIEventRouter() = default;

        UIEventRouter(const UIEventRouter &) = delete;
        UIEventRouter &operator=(const UIEventRouter &) = delete;

        // Topmost visible element under the point, in reverse paint order and
        // clipped by every ancestor that clips its children.
        UIElement *HitTest(float x, float y) const;

        UIEventResult DispatchPointer(const UIPointerEvent &event);
        UIEventResult DispatchKey(const UIKeyEvent &event);
        UIEventResult DispatchText(const UITextInputEvent &event);

        void SetFocus(UIElement *element) { focus = element; }
        UIElement *GetFocus() const { return focus; }

        // Clears focus/hover/capture element pointers. Call before the element
        // tree is destroyed or rebuilt, so no stale pointer is dereferenced.
        void Reset();

        UIElement *GetCapture(uint32_t pointerId = 0) const;

    private:
        static UIElement *HitTestElement(UIElement *element, float x, float y, const struct UIRect &clip);

        void UpdateHover(UIElement *target, const UIPointerEvent &event);
        void SetFocusFrom(UIElement *target);

        UIContext &context;
        UIElement *focus = nullptr;
        UIElement *hovered = nullptr;
        // Capture is per pointer id so simultaneous pointers drag independently.
        std::unordered_map<uint32_t, UIElement *> captures;
    };

} // namespace sky::ui
