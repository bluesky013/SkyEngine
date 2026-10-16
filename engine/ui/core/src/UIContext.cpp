//
// Created on 2026/09/19.
//

#include <ui/UIContext.h>
#include <ui/UIEventRouter.h>
#include <ui/UIPaintContext.h>
#include <ui/animation/UIAnimation.h>

#include <vector>

namespace sky::ui {

    UIContext::UIContext()
    {
        root = std::make_unique<UIElement>();
        root->SetLayoutVolatile(false);
        root->SetName("Root");
        root->inputContext    = this;
        root->contextLifetime = inputLifetime;
    }

    UIContext::~UIContext()
    {
        std::vector<UIElement::InputCancellation> pending;
        root->RenewInputAttachment();
        root->CollectSubtreeInput(pending);
        CollectDetachedInput(root.get(), pending);
        UIElement::QueueDetachedInput(pending);
        inputRouters.clear();
        inputLifetime.reset();
        root->inputContext = nullptr;
        UIElement::CancelDetachedInput(pending);
    }

    void UIContext::CollectDetachedInput(UIElement *subtree, std::vector<UIElement::InputCancellation> &pending)
    {
        // No callbacks until every router has forgotten the removed subtree.
        for (const auto &entry : inputRouters) {
            if (!entry.lifetime.expired()) {
                entry.router->CollectDetachedInput(subtree, pending);
            }
        }
    }

    UIElement *UIContext::AddChild(UIElementPtr child)
    {
        return root->AddChild(std::move(child));
    }

    UIElement *UIContext::FindById(uint32_t id) const
    {
        UIElement *result = nullptr;
        Traverse([&result, id](UIElement *element) {
            if (result == nullptr && element->GetId() == id) {
                result = element;
            }
        });
        return result;
    }

    UIElement *UIContext::FindByName(const std::string &name) const
    {
        UIElement *result = nullptr;
        Traverse([&result, &name](UIElement *element) {
            if (result == nullptr && element->GetName() == name) {
                result = element;
            }
        });
        return result;
    }

    void UIContext::Traverse(const std::function<void(UIElement *)> &fn) const
    {
        TraverseFrom(root.get(), fn);
    }

    UIElement *UIContext::FindNextFocus(UIElement *current, bool forward) const
    {
        std::vector<UIElement *> focusables;
        Traverse([&focusables](UIElement *element) {
            if (element->IsFocusable() && element->IsEnabled() && element->IsEffectivelyVisible()) {
                focusables.push_back(element);
            }
        });

        if (focusables.empty()) {
            return nullptr;
        }

        const int count = static_cast<int>(focusables.size());
        if (current == nullptr) {
            return forward ? focusables.front() : focusables.back();
        }

        int index = -1;
        for (int i = 0; i < count; ++i) {
            if (focusables[i] == current) {
                index = i;
                break;
            }
        }
        if (index < 0) {
            return focusables.front();
        }

        const int next = ((index + (forward ? 1 : -1)) % count + count) % count;
        return focusables[next];
    }

    void UIContext::TraverseFrom(UIElement *element, const std::function<void(UIElement *)> &fn)
    {
        if (element == nullptr) {
            return;
        }
        fn(element);
        for (const auto &child : element->GetChildren()) {
            TraverseFrom(child.get(), fn);
        }
    }

    void UIContext::SetContentSize(float width, float height)
    {
        logicalWidth  = width;
        logicalHeight = height;
        ApplyContentSize();
    }

    void UIContext::SetDpiScale(float value)
    {
        dpiScale = value;
        ApplyContentSize();
    }

    void UIContext::ApplyContentSize()
    {
        UIRect content;
        content.left   = 0.0f;
        content.top    = 0.0f;
        content.right  = logicalWidth * dpiScale;
        content.bottom = logicalHeight * dpiScale;
        root->SetBounds(content);
    }

    void UIContext::Layout()
    {
        if (root == nullptr) {
            return;
        }

        root->ArrangeIfNeeded(root->GetBounds());
    }

    UIAnimation *UIContext::AddAnimation(std::unique_ptr<UIAnimation> animation)
    {
        UIAnimation *raw = animation.get();
        animations.emplace_back(std::move(animation));
        return raw;
    }

    void UIContext::Tick(float delta)
    {
        for (auto it = animations.begin(); it != animations.end();) {
            if ((*it)->Advance(delta)) {
                ++it;
            } else {
                it = animations.erase(it);
            }
        }
    }

    void UIContext::Paint(UIPaintContext &context)
    {
        if (root == nullptr) {
            return;
        }

        context.Begin(root->GetBounds());
        context.SetTheme(&theme);
        root->Paint(context);
    }

    void UIContext::SetSurfaceTransform(const UI2DTransform &value)
    {
        root->surfaceTransform = value;
        root->MarkPaintDirty();
    }

    const UI2DTransform &UIContext::GetSurfaceTransform() const
    {
        return root->surfaceTransform;
    }

    void UIContext::Paint(UIPaintContext &context, const UI2DTransform &rootTransform)
    {
        SetSurfaceTransform(rootTransform);
        Paint(context);
    }

} // namespace sky::ui
