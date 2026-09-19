//
// Created on 2026/09/19.
//

#pragma once

#include <ui/UIElement.h>

#include <cstddef>
#include <functional>
#include <unordered_map>

namespace sky::ui {

    // Vertical virtualized list with fixed item height. Only items in the visible
    // range are materialized through the factory.
    class ListView : public UIElement {
    public:
        using ItemFactory = std::function<UIElementPtr(size_t index)>;

        ListView();
        ~ListView() override = default;

        const char *GetTypeName() const override { return "ListView"; }

        void SetItemCount(size_t count);
        size_t GetItemCount() const { return itemCount; }

        void SetItemHeight(float value);
        float GetItemHeight() const { return itemHeight; }

        void SetItemFactory(ItemFactory value) { factory = std::move(value); }

        void SetScrollY(float value);
        void ScrollBy(float dy);
        float GetScrollY() const { return scrollY; }

        size_t GetLiveItemCount() const { return items.size(); }
        bool IsItemLive(size_t index) const { return items.find(index) != items.end(); }

        UIEventResult OnPointerEvent(const UIPointerEvent &event) override;
        void Measure(float &outWidth, float &outHeight) override;

    protected:
        void ArrangeChildren(const UIRect &view) override;

    private:
        size_t itemCount = 0;
        float itemHeight = 0.0f;
        float scrollY = 0.0f;
        ItemFactory factory;
        std::unordered_map<size_t, UIElement *> items;
    };

} // namespace sky::ui
