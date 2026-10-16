//
// Created on 2026/09/19.
//

#include <ui/widgets/ListView.h>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
#include <vector>

namespace sky::ui {

    namespace {
        float NonNegative(float value)
        {
            return std::isfinite(value) ? std::max(0.0f, value) : 0.0f;
        }
        size_t BoundedIndex(double value, size_t maximum)
        {
            return value >= static_cast<double>(maximum) ? maximum : static_cast<size_t>(std::max(0.0, value));
        }
    } // namespace

    ListView::ListView()
    {
        SetLayoutVolatile(false);
        SetClipsChildren(true);
    }

    void ListView::SetItemCount(size_t count)
    {
        if (itemCount == count) {
            return;
        }
        itemCount = count;
        MarkLayoutDirty();
    }

    void ListView::SetItemHeight(float value)
    {
        const float next = NonNegative(value);
        if (itemHeight == next) {
            return;
        }
        itemHeight = next;
        MarkLayoutDirty();
    }

    void ListView::SetItemFactory(ItemFactory value)
    {
        factory = std::move(value);
        ClearChildren();
        MarkLayoutDirty();
    }

    void ListView::SetScrollY(float value)
    {
        const float next = NonNegative(value);
        if (scrollY == next) {
            return;
        }
        scrollY = next;
        MarkArrangeDirty();
    }

    void ListView::ScrollBy(float dy)
    {
        SetScrollY(scrollY + dy);
    }

    UIEventResult ListView::OnPointerEvent(const UIPointerEvent &event)
    {
        if (event.action == UIPointerAction::WHEEL) {
            ScrollBy(event.wheelDelta);
            return UIEventResult::HANDLED;
        }
        return UIElement::OnPointerEvent(event);
    }

    void ListView::Measure(float &outWidth, float &outHeight)
    {
        const UISize desired = MeasureDesired();
        outWidth             = desired.width;
        outHeight            = desired.height;
    }

    float ListView::GetContentHeight() const
    {
        return static_cast<float>(std::min(static_cast<double>(itemHeight) * itemCount, static_cast<double>(std::numeric_limits<float>::max())));
    }

    UISize ListView::OnMeasure(const UIConstraints &constraints)
    {
        (void)constraints;
        const UILayoutParams &layout = GetLayout();
        return {NonNegative(layout.paddingLeft) + NonNegative(layout.paddingRight),
                GetContentHeight() + NonNegative(layout.paddingTop) + NonNegative(layout.paddingBottom)};
    }

    void ListView::OnChildrenChanged()
    {
        for (auto it = items.begin(); it != items.end();) {
            const auto found = std::find_if(GetChildren().begin(), GetChildren().end(),
                                            [element = it->second](const UIElementPtr &child) { return child.get() == element; });
            it               = found == GetChildren().end() ? items.erase(it) : std::next(it);
        }
    }

    void ListView::ArrangeChildren(const UIRect &view)
    {
        const float  totalHeight = GetContentHeight();
        const UISize viewport    = GetContentSize();
        const float  viewHeight  = viewport.height;
        const float  maxScroll   = std::max(0.0f, totalHeight - viewHeight);
        scrollY                  = std::clamp(scrollY, 0.0f, maxScroll);

        const bool               canDisplay = factory != nullptr && itemHeight > 0.0f && viewHeight > 0.0f;
        const size_t             first      = canDisplay ? BoundedIndex(static_cast<double>(scrollY) / itemHeight, itemCount) : 0;
        const size_t             visible    = canDisplay ? BoundedIndex(static_cast<double>(viewHeight) / itemHeight + 2.0, itemCount - first) : 0;
        const size_t             last       = first + visible;
        std::vector<UIElement *> expired;
        for (const auto &item : items) {
            if (item.first < first || item.first >= last) {
                expired.push_back(item.second);
            }
        }
        for (UIElement *item : expired) {
            RemoveChild(item); // OnChildrenChanged synchronizes the live index.
        }

        for (size_t index = first; index < last; ++index) {
            const auto found = items.find(index);
            if (found == items.end()) {
                UIElementPtr created = factory(index);
                if (created == nullptr) {
                    continue;
                }
                UIElement *item = AddChild(std::move(created));
                items.emplace(index, item);
            }
        }

        // Finish membership changes before arranging: AddChild dirties all siblings.
        for (size_t index = first; index < last; ++index) {
            const auto found = items.find(index);
            if (found == items.end()) {
                continue;
            }
            UIRect slot;
            slot.left   = view.left;
            slot.top    = view.top - scrollY + static_cast<float>(index) * itemHeight;
            slot.right  = view.right;
            slot.bottom = slot.top + itemHeight;
            found->second->Arrange(slot, {viewport.width, itemHeight});
        }
    }

} // namespace sky::ui
