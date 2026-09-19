//
// Created on 2026/09/19.
//

#include <ui/UIElement.h>
#include <ui/UIPaintContext.h>

#include <algorithm>

namespace sky::ui {

    uint32_t UIElement::nextId = 1;

    UIElement::UIElement()
        : id(nextId++)
    {
    }

    UIElement::~UIElement() = default;

    UIElement *UIElement::AddChild(UIElementPtr child)
    {
        if (child == nullptr) {
            return nullptr;
        }

        UIElement *raw = child.get();
        raw->parent = this;
        children.emplace_back(std::move(child));

        MarkLayoutDirty();
        MarkPaintDirty();
        return raw;
    }

    UIElementPtr UIElement::RemoveChild(UIElement *child)
    {
        for (auto it = children.begin(); it != children.end(); ++it) {
            if (it->get() == child) {
                UIElementPtr result = std::move(*it);
                children.erase(it);
                result->parent = nullptr;

                MarkLayoutDirty();
                MarkPaintDirty();
                return result;
            }
        }
        return nullptr;
    }

    bool UIElement::MoveChild(UIElement *child, int delta)
    {
        for (size_t i = 0; i < children.size(); ++i) {
            if (children[i].get() != child) {
                continue;
            }
            const int target = static_cast<int>(i) + delta;
            if (target < 0 || target >= static_cast<int>(children.size())) {
                return false;
            }
            std::swap(children[i], children[static_cast<size_t>(target)]);
            MarkLayoutDirty();
            return true;
        }
        return false;
    }

    void UIElement::ClearChildren()
    {
        children.clear();
        MarkLayoutDirty();
        MarkPaintDirty();
    }

    void UIElement::SetVisible(bool value)
    {
        if (visible == value) {
            return;
        }
        visible = value;
        MarkPaintDirty();
    }

    bool UIElement::IsEffectivelyVisible() const
    {
        const UIElement *element = this;
        while (element != nullptr) {
            if (!element->visible) {
                return false;
            }
            element = element->parent;
        }
        return true;
    }

    void UIElement::SetBounds(const UIRect &value)
    {
        if (value.left == bounds.left && value.top == bounds.top &&
            value.right == bounds.right && value.bottom == bounds.bottom) {
            return;
        }
        bounds = value;
        MarkLayoutDirty();
        MarkPaintDirty();
    }

    void UIElement::MarkLayoutDirty()
    {
        layoutDirty = true;
        for (const auto &child : children) {
            child->MarkLayoutDirty();
        }
    }

    void UIElement::MarkPaintDirty()
    {
        paintDirty = true;
        for (const auto &child : children) {
            child->MarkPaintDirty();
        }
    }

    void UIElement::SetLayout(const UILayoutParams &value)
    {
        layout = value;
        MarkLayoutDirty();
    }

    void UIElement::AddStyleClass(const std::string &className)
    {
        styleClasses.push_back(className);
        MarkPaintDirty();
    }

    void UIElement::Measure(float &outWidth, float &outHeight)
    {
        outWidth  = 0.0f;
        outHeight = 0.0f;
    }

    void UIElement::Layout(const UIRect &parentRect)
    {
        float measuredWidth  = 0.0f;
        float measuredHeight = 0.0f;
        Measure(measuredWidth, measuredHeight);

        SetBounds(ComputeElementBounds(layout, parentRect, measuredWidth, measuredHeight));

        ArrangeChildren(layout.ContentRect(bounds));
        ClearLayoutDirty();
    }

    void UIElement::ArrangeChildren(const UIRect &content)
    {
        for (auto &child : children) {
            child->Layout(content);
        }
    }

    std::vector<UIElement *> UIElement::GetPaintOrder() const
    {
        std::vector<UIElement *> order;
        order.reserve(children.size());
        for (const auto &child : children) {
            order.push_back(child.get());
        }
        std::stable_sort(order.begin(), order.end(), [](const UIElement *a, const UIElement *b) {
            return a->GetLayout().z < b->GetLayout().z;
        });
        return order;
    }

    void UIElement::SetTransform(const UITransform &value)
    {
        transform = value;
        MarkLayoutDirty();
        MarkPaintDirty();
    }

    void UIElement::SetOpacity(float value)
    {
        opacity = value;
        MarkPaintDirty();
    }

    bool UIElement::SetProperty(const std::string &path, const UIPropertyValue &value)
    {
        if (path == "name" && value.type == UIPropertyValue::Type::STRING) {
            SetName(value.stringValue);
            return true;
        }
        if (path == "visible" && value.type == UIPropertyValue::Type::BOOL) {
            SetVisible(value.boolValue);
            return true;
        }
        if (path == "enabled" && value.type == UIPropertyValue::Type::BOOL) {
            SetEnabled(value.boolValue);
            return true;
        }
        return false;
    }

    void UIElement::Paint(UIPaintContext &context)
    {
        if (!IsVisible()) {
            return;
        }

        const bool hasTransform = !transform.IsIdentity();
        const bool hasOpacity = opacity < 1.0f;

        if (hasTransform) {
            context.PushTransform(transform.ToMatrix());
        }
        if (hasOpacity) {
            context.PushOpacity(opacity);
        }
        if (clipsChildren) {
            context.PushClip(bounds);
        }

        OnPaint(context);
        for (UIElement *child : GetPaintOrder()) {
            child->Paint(context);
        }

        if (clipsChildren) {
            context.PopClip();
        }
        if (hasOpacity) {
            context.PopOpacity();
        }
        if (hasTransform) {
            context.PopTransform();
        }
        ClearPaintDirty();
    }

} // namespace sky::ui
