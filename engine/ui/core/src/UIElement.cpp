//
// Created on 2026/09/19.
//

#include <ui/UIContext.h>
#include <ui/UIElement.h>
#include <ui/UIPaintContext.h>

#include <algorithm>

namespace sky::ui {

    namespace {
        bool SameRect(const UIRect &a, const UIRect &b)
        {
            return a.left == b.left && a.top == b.top && a.right == b.right && a.bottom == b.bottom;
        }
        bool SameSize(UISize a, UISize b)
        {
            return a.width == b.width && a.height == b.height;
        }

        bool SameConstraints(const UIConstraints &a, const UIConstraints &b)
        {
            return a.minSize.width == b.minSize.width && a.minSize.height == b.minSize.height && a.maxSize.width == b.maxSize.width &&
                   a.maxSize.height == b.maxSize.height;
        }

        bool SameLayout(const UILayoutParams &a, const UILayoutParams &b)
        {
            return a.anchorMinX == b.anchorMinX && a.anchorMinY == b.anchorMinY && a.anchorMaxX == b.anchorMaxX && a.anchorMaxY == b.anchorMaxY &&
                   a.offsetLeft == b.offsetLeft && a.offsetTop == b.offsetTop && a.offsetRight == b.offsetRight && a.offsetBottom == b.offsetBottom &&
                   a.width.mode == b.width.mode && a.width.value == b.width.value && a.height.mode == b.height.mode &&
                   a.height.value == b.height.value && a.z == b.z && a.paddingLeft == b.paddingLeft && a.paddingTop == b.paddingTop &&
                   a.paddingRight == b.paddingRight && a.paddingBottom == b.paddingBottom;
        }
    } // namespace

    // An element-owned scope, never a DLL-local global. Existing virtual
    // container callbacks inherit the incremental pass through their tree root.
    class UIElement::LayoutPass {
    public:
        explicit LayoutPass(UIElement *element) : root(element->GetTreeRoot()), previous(root->incrementalLayout)
        {
            root->incrementalLayout = true;
        }
        ~LayoutPass()
        {
            root->incrementalLayout = previous;
        }

    private:
        UIElement *root;
        bool       previous;
    };

    uint32_t UIElement::nextId = 1;

    UIElement::UIElement() : id(nextId++)
    {
    }

    UIElement::~UIElement()
    {
        inputLifetime.reset();
        inputAttachment.reset();
        for (auto &child : children) {
            child->parent = nullptr;
        }
    }

    UIElement::InputReference UIElement::GetInputReference() const
    {
        return {const_cast<UIElement *>(this), inputLifetime, inputAttachment};
    }

    void UIElement::RenewInputAttachment()
    {
        inputAttachment = std::make_shared<char>();
        for (auto &child : children) {
            child->RenewInputAttachment();
        }
    }

    UIContext *UIElement::GetInputContext() const
    {
        const UIElement *root = this;
        while (root->parent != nullptr) {
            root = root->parent;
        }
        return !root->contextLifetime.expired() ? root->inputContext : nullptr;
    }

    void UIElement::CollectSubtreeInput(std::vector<InputCancellation> &out)
    {
        // A callback can remove another pending target from the detached tree.
        // Flush that target's old interaction before nested removal returns it.
        for (const auto &entry : pendingInputCancellations) {
            if (auto delivery = entry.lock(); delivery != nullptr && !delivery->delivered) {
                out.push_back({GetInputReference(), delivery->pointerId, delivery});
            }
        }
        CollectDetachedInput(nullptr, out);
        for (auto &child : children) {
            child->CollectSubtreeInput(out);
        }
    }

    void UIElement::QueueDetachedInput(std::vector<InputCancellation> &pending)
    {
        for (auto &entry : pending) {
            UIElement *target = entry.target.Get();
            if (target == nullptr) {
                continue;
            }
            auto &queued = target->pendingInputCancellations;
            queued.erase(std::remove_if(queued.begin(), queued.end(),
                                        [](const auto &weak) {
                                            const auto state = weak.lock();
                                            return state == nullptr || state->delivered;
                                        }),
                         queued.end());
            if (entry.delivery == nullptr) {
                for (const auto &weak : queued) {
                    auto state = weak.lock();
                    if (state->pointerId == entry.pointerId) {
                        entry.delivery = std::move(state);
                        break;
                    }
                }
                if (entry.delivery == nullptr) {
                    entry.delivery = std::make_shared<InputCancellationState>(InputCancellationState{entry.pointerId});
                    queued.push_back(entry.delivery);
                }
            }
        }
    }

    void UIElement::CancelDetachedInput(const std::vector<InputCancellation> &pending)
    {
        for (const auto &entry : pending) {
            UIElement *target = entry.target.Get();
            if (target != nullptr && entry.delivery != nullptr && !entry.delivery->delivered) {
                // Claim before user code; nested removal shares this ticket.
                entry.delivery->delivered = true;
                target->OnPointerCancel(entry.pointerId);
            }
        }
    }

    UIElement *UIElement::AddChild(UIElementPtr child)
    {
        if (child == nullptr) {
            return nullptr;
        }

        UIElement *raw = child.get();
        raw->parent    = this;
        raw->RenewInputAttachment();
        AdjustVolatileCount(raw->volatileCount, true);
        children.emplace_back(std::move(child));
        MarkLayoutDirty();
        MarkPaintDirty();
        const auto reference = raw->GetInputReference();
        OnChildrenChanged();
        return reference.Get();
    }

    UIElementPtr UIElement::RemoveChild(UIElement *child)
    {
        for (auto it = children.begin(); it != children.end(); ++it) {
            if (it->get() == child) {
                UIContext   *context = GetInputContext();
                UIElementPtr result  = std::move(*it);
                children.erase(it);
                AdjustVolatileCount(result->volatileCount, false);
                result->parent = nullptr;
                result->RenewInputAttachment();
                result->MarkLayoutDirty();
                MarkLayoutDirty();
                MarkPaintDirty();
                std::vector<InputCancellation> pending;
                CollectDetachedInput(result.get(), pending);
                result->CollectSubtreeInput(pending);
                if (context != nullptr) {
                    context->CollectDetachedInput(result.get(), pending);
                }
                QueueDetachedInput(pending);
                OnChildrenChanged();
                CancelDetachedInput(pending);
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
            OnChildrenChanged();
            return true;
        }
        return false;
    }

    void UIElement::ClearChildren()
    {
        UIContext                *context = GetInputContext();
        std::vector<UIElementPtr> removed;
        removed.swap(children);
        std::vector<InputCancellation> pending;
        for (const auto &child : removed) {
            AdjustVolatileCount(child->volatileCount, false);
            child->parent = nullptr;
            child->RenewInputAttachment();
            child->MarkLayoutDirty();
            CollectDetachedInput(child.get(), pending);
            child->CollectSubtreeInput(pending);
            if (context != nullptr) {
                context->CollectDetachedInput(child.get(), pending);
            }
        }
        MarkLayoutDirty();
        MarkPaintDirty();
        QueueDetachedInput(pending);
        OnChildrenChanged();
        CancelDetachedInput(pending);
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
        const UISize size = UIConstraints{}.Constrain({value.Width(), value.Height()});
        if (SameRect(value, bounds) && SameSize(size, arrangedSize)) {
            return;
        }
        bounds         = value;
        arrangedSize   = size;
        hasLayoutInput = false;
        MarkArrangeDirty();
    }

    UIGeometry UIElement::GetGeometry(const UI2DTransform &surfaceTransform) const
    {
        const UI2DTransform inherited =
            parent != nullptr ? parent->GetGeometry(surfaceTransform).layoutToSurface : surfaceTransform * this->surfaceTransform;
        return {bounds, inherited * transform.ToMatrix()};
    }

    void UIElement::MarkLayoutDirty()
    {
        // Even an already-dirty ancestor may hold a query result from before
        // this mutation. Always invalidate it; do not stop at a dirty flag.
        for (UIElement *element = this; element != nullptr; element = element->parent) {
            ++element->measureGeneration;
            element->hasLayoutInput = false;
            for (auto &entry : element->measureCache) {
                entry.valid = false;
            }
        }
        MarkArrangeDirty();
    }

    void UIElement::MarkArrangeDirty()
    {
        for (UIElement *element = this; element != nullptr; element = element->parent) {
            element->layoutDirty = true;
            ++element->layoutGeneration;
        }
        MarkPaintDirty();
    }

    void UIElement::MarkPaintDirty()
    {
        for (UIElement *element = this; element != nullptr; element = element->parent) {
            element->paintDirty = true;
            ++element->paintGeneration;
        }
    }

    UIElement *UIElement::GetTreeRoot()
    {
        UIElement *root = this;
        while (root->parent != nullptr) {
            root = root->parent;
        }
        return root;
    }

    void UIElement::AdjustVolatileCount(size_t count, bool add)
    {
        for (UIElement *element = this; element != nullptr; element = element->parent) {
            if (add) {
                element->volatileCount += count;
            } else {
                element->volatileCount -= count;
            }
        }
    }

    void UIElement::SetLayoutVolatile(bool value)
    {
        if (layoutVolatile == value) {
            return;
        }
        layoutVolatile = value;
        AdjustVolatileCount(1, value);
        MarkLayoutDirty();
    }

    void UIElement::SetLayout(const UILayoutParams &value)
    {
        if (SameLayout(layout, value)) {
            return;
        }
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

    UISize UIElement::OnMeasure(const UIConstraints &constraints)
    {
        (void)constraints;
        UISize desired;
        Measure(desired.width, desired.height);
        return desired;
    }

    UISize UIElement::MeasureDesired(const UIConstraints &constraints)
    {
        const UIConstraints normalized = constraints.Normalized();
        if (CanRetainLayout()) {
            for (const auto &entry : measureCache) {
                if (entry.valid && SameConstraints(entry.constraints, normalized)) {
                    return entry.size;
                }
            }
        }
        const uint64_t generation = measureGeneration;
        const UISize   desired    = normalized.Constrain(OnMeasure(normalized));
        if (CanRetainLayout() && generation == measureGeneration) {
            measureCache[nextMeasureEntry] = {normalized, desired, true};
            nextMeasureEntry               = (nextMeasureEntry + 1) % measureCache.size();
        }
        return desired;
    }

    UISize UIElement::MeasureForLayout(const UIConstraints &available)
    {
        return MeasureDesired(ResolveMeasureConstraints(layout, available));
    }

    void UIElement::Layout(const UIRect &parentRect)
    {
        Layout(parentRect, {parentRect.Width(), parentRect.Height()});
    }

    void UIElement::Layout(const UIRect &parentRect, UISize parentSize)
    {
        parentSize = UIConstraints{}.Constrain(parentSize);
        if (GetTreeRoot()->incrementalLayout && CanRetainLayout() && !layoutDirty && arranged && hasLayoutInput &&
            SameRect(lastLayoutInput, parentRect) && SameSize(lastLayoutInputSize, parentSize) && SameRect(lastLayoutResult, bounds) &&
            SameSize(lastLayoutResultSize, arrangedSize)) {
            return;
        }
        const uint64_t generation = layoutGeneration;
        const UISize   desired    = MeasureForLayout({{}, parentSize});
        Arrange(ComputeElementBounds(layout, parentRect, parentSize, desired.width, desired.height), desired);
        if (generation != layoutGeneration) {
            // A callback can schedule another pass before Arrange starts, too.
            layoutDirty    = true;
            hasLayoutInput = false;
        } else {
            lastLayoutInput      = parentRect;
            lastLayoutInputSize  = parentSize;
            lastLayoutResult     = bounds;
            lastLayoutResultSize = arrangedSize;
            hasLayoutInput       = true;
        }
    }

    void UIElement::Arrange(const UIRect &finalRect)
    {
        Arrange(finalRect, {finalRect.Width(), finalRect.Height()});
    }

    void UIElement::Arrange(const UIRect &finalRect, UISize logicalSize)
    {
        logicalSize           = UIConstraints{}.Constrain(logicalSize);
        const bool sameBounds = SameRect(bounds, finalRect) && SameSize(arrangedSize, logicalSize);
        if (GetTreeRoot()->incrementalLayout && CanRetainLayout() && !layoutDirty && arranged && sameBounds) {
            return;
        }
        if (!sameBounds) {
            // Final allocation is an output, not a new intrinsic-size request.
            bounds         = finalRect;
            arrangedSize   = logicalSize;
            hasLayoutInput = false;
            MarkPaintDirty();
        }
        const uint64_t generation = layoutGeneration;
        ArrangeChildren(layout.ContentRect(bounds));
        arranged = true;
        if (generation == layoutGeneration) {
            ClearLayoutDirty();
        }
    }

    void UIElement::LayoutIfNeeded(const UIRect &parentRect)
    {
        LayoutIfNeeded(parentRect, {parentRect.Width(), parentRect.Height()});
    }

    void UIElement::LayoutIfNeeded(const UIRect &parentRect, UISize parentSize)
    {
        LayoutPass pass(this);
        Layout(parentRect, parentSize);
    }

    void UIElement::ArrangeIfNeeded(const UIRect &finalRect)
    {
        ArrangeIfNeeded(finalRect, {finalRect.Width(), finalRect.Height()});
    }

    void UIElement::ArrangeIfNeeded(const UIRect &finalRect, UISize logicalSize)
    {
        LayoutPass pass(this);
        Arrange(finalRect, logicalSize);
    }

    void UIElement::ArrangeChildren(const UIRect &content)
    {
        for (auto &child : children) {
            child->Layout(content, GetContentSize());
        }
    }

    std::vector<UIElement *> UIElement::GetPaintOrder() const
    {
        std::vector<UIElement *> order;
        order.reserve(children.size());
        for (const auto &child : children) {
            order.push_back(child.get());
        }
        std::stable_sort(order.begin(), order.end(), [](const UIElement *a, const UIElement *b) { return a->GetLayout().z < b->GetLayout().z; });
        return order;
    }

    void UIElement::SetTransform(const UITransform &value)
    {
        if (transform.translationX == value.translationX && transform.translationY == value.translationY && transform.rotation == value.rotation &&
            transform.scaleX == value.scaleX && transform.scaleY == value.scaleY && transform.pivotX == value.pivotX &&
            transform.pivotY == value.pivotY) {
            return;
        }
        transform = value;
        MarkPaintDirty();
    }

    void UIElement::SetOpacity(float value)
    {
        if (opacity == value) {
            return;
        }
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

        const uint64_t generation = paintGeneration;

        const bool hasTransform = !transform.IsIdentity();
        const bool hasOpacity   = opacity < 1.0f;

        if (parent == nullptr) {
            context.PushTransform(surfaceTransform);
        }

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
        if (parent == nullptr) {
            context.PopTransform();
        }
        if (generation == paintGeneration) {
            ClearPaintDirty();
        }
    }

} // namespace sky::ui
