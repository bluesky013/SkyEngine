//
// Created on 2026/09/19.
//

#pragma once

#include <ui/UIElement.h>
#include <ui/UIStyle.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace sky::ui {

    class UIPaintContext;
    class UIAnimation;

    class UIContext {
    public:
        UIContext();
        ~UIContext();

        UIContext(const UIContext &) = delete;
        UIContext &operator=(const UIContext &) = delete;

        UIElement *GetRoot() const { return root.get(); }

        // Top-level elements are always attached to the single root.
        UIElement *AddChild(UIElementPtr child);

        UIElement *FindById(uint32_t id) const;
        UIElement *FindByName(const std::string &name) const;

        // Next/previous focusable element (enabled, effectively visible) in
        // traversal order with wrap-around. Pass null to start from an end.
        UIElement *FindNextFocus(UIElement *current, bool forward) const;

        // Depth-first pre-order: parent before children, children in insertion order.
        void Traverse(const std::function<void(UIElement *)> &fn) const;

        void SetContentSize(float width, float height);

        // Scales the logical content size into device coordinates.
        void SetDpiScale(float value);
        float GetDpiScale() const { return dpiScale; }

        UITheme &GetTheme() { return theme; }
        const UITheme &GetTheme() const { return theme; }

        // Framework reads this to gate game input while a UI surface is active.
        void SetWantsInput(bool value) { wantsInput = value; }
        bool WantsInput() const { return wantsInput; }

        // Arranges top-level elements inside the root's content rect.
        void Layout();
        void Paint(UIPaintContext &context);

        UIAnimation *AddAnimation(std::unique_ptr<UIAnimation> animation);
        // Advances animations and removes completed ones.
        void Tick(float delta);
        size_t GetAnimationCount() const { return animations.size(); }

    private:
        static void TraverseFrom(UIElement *element, const std::function<void(UIElement *)> &fn);
        void ApplyContentSize();

        UIElementPtr root;
        UITheme theme;
        bool wantsInput = false;
        std::vector<std::unique_ptr<UIAnimation>> animations;
        float logicalWidth = 0.0f;
        float logicalHeight = 0.0f;
        float dpiScale = 1.0f;
    };

} // namespace sky::ui
