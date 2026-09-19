//
// Created on 2026/09/19.
//

#include <gtest/gtest.h>
#include <ui/UIContext.h>
#include <ui/UILayout.h>
#include <ui/UIPaintContext.h>
#include <ui/widgets/Image.h>
#include <ui/widgets/ListView.h>
#include <ui/widgets/ScrollView.h>

namespace sky::ui {

    namespace {

        UILayoutParams Fixed(float width, float height)
        {
            UILayoutParams params;
            params.width.mode = UISizeMode::FIXED;
            params.width.value = width;
            params.height.mode = UISizeMode::FIXED;
            params.height.value = height;
            return params;
        }

    } // namespace

    TEST(UIScrollTest, ScrollClampsToContent)
    {
        UIContext context;
        context.SetContentSize(50.0f, 40.0f);

        auto holder = std::make_unique<ScrollView>();
        auto *view = holder.get();
        context.AddChild(std::move(holder));
        view->SetLayout(Fixed(50.0f, 40.0f));

        auto content = std::make_unique<UIElement>();
        content->SetLayout(Fixed(50.0f, 100.0f));
        view->SetContent(std::move(content));

        context.Layout();
        view->ScrollBy(0.0f, 200.0f);
        context.Layout();
        EXPECT_FLOAT_EQ(view->GetScrollY(), 60.0f);

        view->ScrollBy(0.0f, -500.0f);
        context.Layout();
        EXPECT_FLOAT_EQ(view->GetScrollY(), 0.0f);
    }

    TEST(UIScrollTest, WheelScrolls)
    {
        UIContext context;
        context.SetContentSize(50.0f, 40.0f);

        auto holder = std::make_unique<ScrollView>();
        auto *view = holder.get();
        context.AddChild(std::move(holder));
        view->SetLayout(Fixed(50.0f, 40.0f));

        auto content = std::make_unique<UIElement>();
        content->SetLayout(Fixed(50.0f, 100.0f));
        view->SetContent(std::move(content));

        UIPointerEvent event;
        event.action = UIPointerAction::WHEEL;
        event.wheelDelta = 15.0f;
        EXPECT_EQ(view->OnPointerEvent(event), UIEventResult::HANDLED);

        context.Layout();
        EXPECT_FLOAT_EQ(view->GetScrollY(), 15.0f);
    }

    TEST(UIScrollTest, ContentIsClipped)
    {
        UIContext context;
        context.SetContentSize(50.0f, 40.0f);

        auto holder = std::make_unique<ScrollView>();
        auto *view = holder.get();
        context.AddChild(std::move(holder));
        view->SetLayout(Fixed(50.0f, 40.0f));

        auto image = std::make_unique<Image>();
        image->SetTexture(1);
        image->SetLayout(Fixed(50.0f, 100.0f));
        view->SetContent(std::move(image));

        context.Layout();

        UIPaintContext paint;
        context.Paint(paint);

        ASSERT_FALSE(paint.GetDrawData().commands.empty());
        EXPECT_FLOAT_EQ(paint.GetDrawData().commands[0].clip.bottom, 40.0f);
    }

    TEST(UIScrollTest, ListViewVirtualizes)
    {
        UIContext context;
        context.SetContentSize(100.0f, 50.0f);

        auto holder = std::make_unique<ListView>();
        auto *list = holder.get();
        context.AddChild(std::move(holder));
        list->SetLayout(Fixed(100.0f, 50.0f));
        list->SetItemHeight(10.0f);
        list->SetItemCount(1000);
        list->SetItemFactory([](size_t) {
            auto item = std::make_unique<UIElement>();
            item->SetLayout(Fixed(100.0f, 10.0f));
            return item;
        });

        context.Layout();

        EXPECT_LE(list->GetLiveItemCount(), 8u);
        EXPECT_TRUE(list->IsItemLive(0));
        EXPECT_FALSE(list->IsItemLive(900));
    }

    TEST(UIScrollTest, ListViewScrollShiftsRange)
    {
        UIContext context;
        context.SetContentSize(100.0f, 50.0f);

        auto holder = std::make_unique<ListView>();
        auto *list = holder.get();
        context.AddChild(std::move(holder));
        list->SetLayout(Fixed(100.0f, 50.0f));
        list->SetItemHeight(10.0f);
        list->SetItemCount(1000);
        list->SetItemFactory([](size_t) {
            auto item = std::make_unique<UIElement>();
            item->SetLayout(Fixed(100.0f, 10.0f));
            return item;
        });

        context.Layout();
        list->ScrollBy(100.0f);
        context.Layout();

        EXPECT_FALSE(list->IsItemLive(0));
        EXPECT_TRUE(list->IsItemLive(10));
    }

} // namespace sky::ui
