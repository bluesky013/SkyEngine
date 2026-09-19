//
// Created on 2026/09/19.
//

#include <ui/widgets/ListView.h>

#include <algorithm>

namespace sky::ui {

    ListView::ListView()
    {
        SetClipsChildren(true);
    }

    void ListView::SetItemCount(size_t count)
    {
        itemCount = count;
        MarkLayoutDirty();
    }

    void ListView::SetItemHeight(float value)
    {
        itemHeight = value;
        MarkLayoutDirty();
    }

    void ListView::SetScrollY(float value)
    {
        scrollY = value;
        MarkLayoutDirty();
    }

    void ListView::ScrollBy(float dy)
    {
        scrollY += dy;
        MarkLayoutDirty();
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
        outWidth = 0.0f;
        outHeight = itemHeight * static_cast<float>(itemCount);
    }

    void ListView::ArrangeChildren(const UIRect &view)
    {
        const float totalHeight = itemHeight * static_cast<float>(itemCount);
        const float maxScroll = std::max(0.0f, totalHeight - view.Height());
        scrollY = std::clamp(scrollY, 0.0f, maxScroll);

        if (factory == nullptr || itemHeight <= 0.0f) {
            return;
        }

        const size_t first = static_cast<size_t>(scrollY / itemHeight);
        const size_t visible = static_cast<size_t>(view.Height() / itemHeight) + 2;
        const size_t last = std::min(itemCount, first + visible);

        for (auto it = items.begin(); it != items.end();) {
            if (it->first < first || it->first >= last) {
                RemoveChild(it->second);
                it = items.erase(it);
            } else {
                ++it;
            }
        }

        for (size_t index = first; index < last; ++index) {
            UIElement *item = nullptr;
            const auto found = items.find(index);
            if (found == items.end()) {
                UIElementPtr created = factory(index);
                if (created == nullptr) {
                    continue;
                }
                item = AddChild(std::move(created));
                items.emplace(index, item);
            } else {
                item = found->second;
            }

            UIRect slot;
            slot.left = view.left;
            slot.top = view.top - scrollY + static_cast<float>(index) * itemHeight;
            slot.right = view.right;
            slot.bottom = slot.top + itemHeight;
            item->Layout(slot);
        }
    }

} // namespace sky::ui
