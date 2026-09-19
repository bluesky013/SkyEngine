//
// Created on 2026/09/19.
//

#pragma once

#include <ui/UIEvent.h>
#include <ui/UILayout.h>
#include <ui/UIRect.h>
#include <ui/UITransform.h>
#include <ui/data/UIProperty.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace sky::ui {

    class UIElement;
    class UIPaintContext;
    using UIElementPtr = std::unique_ptr<UIElement>;

    class UIElement {
    public:
        UIElement();
        virtual ~UIElement();

        UIElement(const UIElement &) = delete;
        UIElement &operator=(const UIElement &) = delete;

        uint32_t GetId() const { return id; }

        const std::string &GetName() const { return name; }
        void SetName(const std::string &value) { name = value; }

        UIElement *GetParent() const { return parent; }

        UIElement *AddChild(UIElementPtr child);
        UIElementPtr RemoveChild(UIElement *child);
        void ClearChildren();
        // Reorders a direct child among its siblings; delta is the index shift.
        bool MoveChild(UIElement *child, int delta);

        // Document type key (overridden by concrete widgets).
        virtual const char *GetTypeName() const { return "UIElement"; }
        const std::vector<UIElementPtr> &GetChildren() const { return children; }

        void SetVisible(bool value);
        bool IsVisible() const { return visible; }
        bool IsEffectivelyVisible() const;

        void SetEnabled(bool value) { enabled = value; }
        bool IsEnabled() const { return enabled; }

        void SetBounds(const UIRect &value);
        const UIRect &GetBounds() const { return bounds; }

        // Marking self also marks descendants: moving a parent invalidates
        // every child position, and a repaint of a parent covers its subtree.
        void MarkLayoutDirty();
        void MarkPaintDirty();

        void ClearLayoutDirty() { layoutDirty = false; }
        void ClearPaintDirty() { paintDirty = false; }
        bool IsLayoutDirty() const { return layoutDirty; }
        bool IsPaintDirty() const { return paintDirty; }

        void SetLayout(const UILayoutParams &value);
        const UILayoutParams &GetLayout() const { return layout; }

        // Children ordered by ascending z (stable for equal z). Paint walks this
        // order; hit-testing walks it in reverse.
        std::vector<UIElement *> GetPaintOrder() const;

        void AddStyleClass(const std::string &className);
        const std::vector<std::string> &GetStyleClasses() const { return styleClasses; }

        // AUTO sizes come from Measure; the base element has no intrinsic size.
        virtual void Measure(float &outWidth, float &outHeight);
        void Layout(const UIRect &parentRect);

    protected:
        // Arranges this element's children inside its content rect. Containers
        // override this; the base arranges every child against the same rect.
        virtual void ArrangeChildren(const UIRect &content);

    public:

        virtual void OnPaint(UIPaintContext &context) { (void)context; }
        void Paint(UIPaintContext &context);

        virtual UIEventResult OnPointerEvent(const UIPointerEvent &event) { (void)event; return UIEventResult::UNHANDLED; }
        virtual UIEventResult OnKeyEvent(const UIKeyEvent &event) { (void)event; return UIEventResult::UNHANDLED; }
        virtual UIEventResult OnTextInput(const UITextInputEvent &event) { (void)event; return UIEventResult::UNHANDLED; }
        virtual void OnPointerEnter(const UIPointerEvent &event) { (void)event; }
        virtual void OnPointerLeave(const UIPointerEvent &event) { (void)event; }

        void SetFocusable(bool value) { focusable = value; }
        bool IsFocusable() const { return focusable; }

        void SetClipsChildren(bool value) { clipsChildren = value; }
        bool ClipsChildren() const { return clipsChildren; }

        void SetTransform(const UITransform &value);
        const UITransform &GetTransform() const { return transform; }

        void SetOpacity(float value);
        float GetOpacity() const { return opacity; }

        // Generic property path applied by the document loader (e.g. "visible",
        // "visual.texture"). Widgets override to extend. Returns false if unknown.
        virtual bool SetProperty(const std::string &path, const UIPropertyValue &value);

    private:
        static uint32_t nextId;

        uint32_t id = 0;
        std::string name;
        UIElement *parent = nullptr;
        std::vector<UIElementPtr> children;
        UIRect bounds;
        UILayoutParams layout;
        std::vector<std::string> styleClasses;
        bool visible = true;
        bool enabled = true;
        bool clipsChildren = false;
        bool focusable = false;
        UITransform transform;
        float opacity = 1.0f;
        bool layoutDirty = true;
        bool paintDirty = true;
    };

} // namespace sky::ui
