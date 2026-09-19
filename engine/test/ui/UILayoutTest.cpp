//
// Created on 2026/09/19.
//

#include <gtest/gtest.h>
#include <ui/UIContext.h>
#include <ui/UIElement.h>
#include <ui/UILayout.h>
#include <ui/widgets/HBox.h>
#include <ui/widgets/VBox.h>

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

    TEST(UILayoutTest, PercentWidthResolvesAgainstParent)
    {
        UIContext context;
        context.SetContentSize(200.0f, 200.0f);

        UIElement *child = context.AddChild(std::make_unique<UIElement>());
        UILayoutParams params;
        params.width.mode = UISizeMode::PERCENT;
        params.width.value = 0.5f;
        params.height.mode = UISizeMode::FIXED;
        params.height.value = 10.0f;
        child->SetLayout(params);

        context.Layout();
        EXPECT_FLOAT_EQ(child->GetBounds().Width(), 100.0f);
    }

    TEST(UILayoutTest, ZOrderControlsPaintOrder)
    {
        UIContext context;
        context.SetContentSize(100.0f, 100.0f);

        auto firstHolder = std::make_unique<UIElement>();
        auto *first = firstHolder.get();
        UILayoutParams firstParams = Fixed(10.0f, 10.0f);
        firstParams.z = 5.0f;
        first->SetLayout(firstParams);

        auto secondHolder = std::make_unique<UIElement>();
        auto *second = secondHolder.get();
        UILayoutParams secondParams = Fixed(10.0f, 10.0f);
        secondParams.z = 1.0f;
        second->SetLayout(secondParams);

        UIElement *parent = context.AddChild(std::make_unique<UIElement>());
        parent->AddChild(std::move(firstHolder));
        parent->AddChild(std::move(secondHolder));

        const std::vector<UIElement *> order = parent->GetPaintOrder();
        ASSERT_EQ(order.size(), 2u);
        EXPECT_EQ(order[0], second);
        EXPECT_EQ(order[1], first);
    }

    TEST(UILayoutTest, HBoxPlacesChildrenInRow)
    {
        UIContext context;
        context.SetContentSize(200.0f, 100.0f);

        auto boxHolder = std::make_unique<HBox>();
        auto *box = boxHolder.get();
        context.AddChild(std::move(boxHolder));
        box->SetLayout(Fixed(100.0f, 20.0f));
        box->SetSpacing(5.0f);

        auto firstHolder = std::make_unique<UIElement>();
        auto *first = firstHolder.get();
        first->SetLayout(Fixed(10.0f, 10.0f));
        box->AddChild(std::move(firstHolder));

        auto secondHolder = std::make_unique<UIElement>();
        auto *second = secondHolder.get();
        second->SetLayout(Fixed(10.0f, 10.0f));
        box->AddChild(std::move(secondHolder));

        context.Layout();

        EXPECT_FLOAT_EQ(first->GetBounds().left, 0.0f);
        EXPECT_FLOAT_EQ(first->GetBounds().right, 10.0f);
        EXPECT_FLOAT_EQ(second->GetBounds().left, 15.0f);
    }

    TEST(UILayoutTest, VBoxMeasuredHeightCoversChildren)
    {
        VBox box;
        UILayoutParams params;
        params.paddingTop = 2.0f;
        params.paddingBottom = 2.0f;
        box.SetLayout(params);
        box.SetSpacing(5.0f);

        box.AddChild(std::make_unique<UIElement>())->SetLayout(Fixed(10.0f, 10.0f));
        box.AddChild(std::make_unique<UIElement>())->SetLayout(Fixed(10.0f, 10.0f));

        float width = 0.0f;
        float height = 0.0f;
        box.Measure(width, height);

        EXPECT_FLOAT_EQ(height, 29.0f);
    }

} // namespace sky::ui
