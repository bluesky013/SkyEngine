//
// Created on 2026/09/19.
//

#include <gtest/gtest.h>
#include <ui/UIContext.h>
#include <ui/UILayout.h>
#include <ui/UIPaintContext.h>
#include <ui/data/UIBuilder.h>
#include <ui/data/UIDocument.h>
#include <ui/data/UIElementRegistry.h>
#include <ui/widgets/Image.h>
#include <ui/widgets/ListView.h>
#include <ui/widgets/ScrollView.h>
#include <ui/widgets/Text.h>

#include <cmath>
#include <limits>

namespace sky::ui {

    namespace {

        UILayoutParams Fixed(float width, float height)
        {
            UILayoutParams params;
            params.width.mode   = UISizeMode::FIXED;
            params.width.value  = width;
            params.height.mode  = UISizeMode::FIXED;
            params.height.value = height;
            return params;
        }

        class ResponsiveContent : public UIElement {
        protected:
            UISize OnMeasure(const UIConstraints &constraints) override
            {
                const float width = std::isfinite(constraints.maxSize.width) ? constraints.maxSize.width : 300.0f;
                return {width, 6000.0f / std::max(1.0f, width)};
            }
        };

    } // namespace

    TEST(UIScrollTest, ScrollClampsToContent)
    {
        UIContext context;
        context.SetContentSize(50.0f, 40.0f);

        auto  holder = std::make_unique<ScrollView>();
        auto *view   = holder.get();
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

        auto  holder = std::make_unique<ScrollView>();
        auto *view   = holder.get();
        context.AddChild(std::move(holder));
        view->SetLayout(Fixed(50.0f, 40.0f));

        auto content = std::make_unique<UIElement>();
        content->SetLayout(Fixed(50.0f, 100.0f));
        view->SetContent(std::move(content));

        UIPointerEvent event;
        event.action     = UIPointerAction::WHEEL;
        event.wheelDelta = 15.0f;
        EXPECT_EQ(view->OnPointerEvent(event), UIEventResult::HANDLED);

        context.Layout();
        EXPECT_FLOAT_EQ(view->GetScrollY(), 15.0f);
    }

    TEST(UIScrollTest, ContentIsClipped)
    {
        UIContext context;
        context.SetContentSize(50.0f, 40.0f);

        auto  holder = std::make_unique<ScrollView>();
        auto *view   = holder.get();
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

        auto  holder = std::make_unique<ListView>();
        auto *list   = holder.get();
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

        auto  holder = std::make_unique<ListView>();
        auto *list   = holder.get();
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

    TEST(UIScrollTest, PercentAndFillContentResolveOnceAgainstViewport)
    {
        ScrollView     view;
        auto           content = std::make_unique<UIElement>();
        UIElement     *raw     = content.get();
        UILayoutParams layout;
        layout.width.mode = UISizeMode::FILL;
        layout.height     = {UISizeMode::PERCENT, 0.5f};
        content->SetLayout(layout);
        view.SetContent(std::move(content));
        view.SetScroll(500.0f, 500.0f);
        view.Arrange({10.0f, 20.0f, 110.0f, 60.0f});
        EXPECT_FLOAT_EQ(view.GetContentWidth(), 100.0f);
        EXPECT_FLOAT_EQ(view.GetContentHeight(), 20.0f);
        EXPECT_FLOAT_EQ(raw->GetBounds().Width(), view.GetContentWidth());
        EXPECT_FLOAT_EQ(raw->GetBounds().Height(), view.GetContentHeight());
        EXPECT_FLOAT_EQ(view.GetScrollX(), 0.0f);
        EXPECT_FLOAT_EQ(view.GetScrollY(), 0.0f);
        view.Arrange({10.0f, 20.0f, 210.0f, 100.0f});
        EXPECT_FLOAT_EQ(raw->GetBounds().Width(), 200.0f);
        EXPECT_FLOAT_EQ(raw->GetBounds().Height(), 40.0f);
    }

    TEST(UIScrollTest, VerticalContentMeasuresAtViewportWidthAndReclampsOnResize)
    {
        ScrollView view;
        view.SetScrollAxes(ScrollAxes::Vertical);
        UIElement *content = view.SetContent(std::make_unique<ResponsiveContent>());
        view.Arrange({0.0f, 0.0f, 100.0f, 20.0f});
        EXPECT_FLOAT_EQ(view.GetContentWidth(), 100.0f);
        EXPECT_FLOAT_EQ(view.GetContentHeight(), 60.0f);
        view.SetScroll(100.0f, 500.0f);
        view.Arrange(view.GetBounds());
        EXPECT_FLOAT_EQ(view.GetScrollX(), 0.0f);
        EXPECT_FLOAT_EQ(view.GetScrollY(), 40.0f);
        EXPECT_FLOAT_EQ(content->GetBounds().top, -40.0f);
        view.Arrange({0.0f, 0.0f, 200.0f, 20.0f});
        EXPECT_FLOAT_EQ(view.GetContentHeight(), 30.0f);
        EXPECT_FLOAT_EQ(view.GetScrollY(), 10.0f);
        EXPECT_FLOAT_EQ(content->GetBounds().Width(), 200.0f);
        EXPECT_FLOAT_EQ(content->GetBounds().Height(), 30.0f);
    }

    TEST(UIScrollTest, NonScrollingAxisHardLimitsExplicitFixedContent)
    {
        ScrollView view;
        view.SetScrollAxes(ScrollAxes::Vertical);
        auto           content = std::make_unique<ResponsiveContent>();
        UILayoutParams layout;
        layout.width = {UISizeMode::FIXED, 500.0f};
        content->SetLayout(layout);
        UIElement *raw = view.SetContent(std::move(content));
        view.Arrange({0.0f, 0.0f, 100.0f, 20.0f});
        EXPECT_FLOAT_EQ(raw->GetBounds().Width(), 100.0f);
        EXPECT_FLOAT_EQ(raw->GetBounds().Height(), 60.0f);
        EXPECT_FLOAT_EQ(view.GetContentWidth(), 100.0f);
        EXPECT_FLOAT_EQ(view.GetContentHeight(), 60.0f);
    }

    TEST(UIScrollTest, AutoViewportRemeasuresHeightAtItsFinalWidth)
    {
        ScrollView view;
        view.SetScrollAxes(ScrollAxes::Vertical);
        auto           content = std::make_unique<ResponsiveContent>();
        UILayoutParams layout;
        layout.width = {UISizeMode::PERCENT, 0.5f};
        content->SetLayout(layout);
        UIElement   *raw     = view.SetContent(std::move(content));
        const UISize desired = view.MeasureDesired({{}, {200.0f, 1000.0f}});
        EXPECT_FLOAT_EQ(desired.width, 100.0f);
        EXPECT_FLOAT_EQ(desired.height, 120.0f);
        view.Arrange({0.0f, 0.0f, desired.width, desired.height});
        EXPECT_FLOAT_EQ(raw->GetBounds().Width(), 50.0f);
        EXPECT_FLOAT_EQ(raw->GetBounds().Height(), desired.height);
        EXPECT_FLOAT_EQ(view.GetContentHeight(), desired.height);
        view.SetScroll(0.0f, 1000.0f);
        view.Arrange(view.GetBounds());
        EXPECT_FLOAT_EQ(view.GetScrollY(), 0.0f);
    }

    TEST(UIScrollTest, HorizontalAutoViewportRemeasuresWidthAtItsFinalHeight)
    {
        class HeightResponsiveContent : public UIElement {
        protected:
            UISize OnMeasure(const UIConstraints &constraints) override
            {
                const float height = std::isfinite(constraints.maxSize.height) ? constraints.maxSize.height : 300.0f;
                return {6000.0f / std::max(1.0f, height), height};
            }
        };
        ScrollView view;
        view.SetScrollAxes(ScrollAxes::Horizontal);
        auto           content = std::make_unique<HeightResponsiveContent>();
        UILayoutParams layout;
        layout.height = {UISizeMode::PERCENT, 0.5f};
        content->SetLayout(layout);
        UIElement   *raw     = view.SetContent(std::move(content));
        const UISize desired = view.MeasureDesired({{}, {1000.0f, 200.0f}});
        EXPECT_FLOAT_EQ(desired.width, 120.0f);
        EXPECT_FLOAT_EQ(desired.height, 100.0f);
        view.Arrange({0.0f, 0.0f, desired.width, desired.height});
        EXPECT_FLOAT_EQ(raw->GetBounds().Width(), desired.width);
        EXPECT_FLOAT_EQ(raw->GetBounds().Height(), 50.0f);
        EXPECT_FLOAT_EQ(view.GetContentWidth(), desired.width);
    }

    TEST(UIScrollTest, MeasuringScrollDoesNotArrangeOrChangeStoredScrollAndExtent)
    {
        ScrollView view;
        view.SetScrollAxes(ScrollAxes::Vertical);
        UIElement *content = view.SetContent(std::make_unique<ResponsiveContent>());
        view.Arrange({5.0f, 10.0f, 105.0f, 30.0f});
        view.SetScroll(0.0f, 25.0f);
        const UIRect previous = content->GetBounds();
        const bool   dirty    = view.IsLayoutDirty();
        const auto   desired  = view.MeasureDesired({{}, {50.0f, 10.0f}});
        EXPECT_FLOAT_EQ(desired.height, 10.0f);
        EXPECT_FLOAT_EQ(view.GetScrollY(), 25.0f);
        EXPECT_FLOAT_EQ(view.GetContentHeight(), 60.0f);
        EXPECT_FLOAT_EQ(content->GetBounds().top, previous.top);
        EXPECT_FLOAT_EQ(content->GetBounds().Width(), previous.Width());
        EXPECT_EQ(view.IsLayoutDirty(), dirty);
    }

    TEST(UIScrollTest, InheritedRemovalAndClearDropStaleContent)
    {
        ScrollView view;
        UIElement *content = view.SetContent(std::make_unique<ResponsiveContent>());
        view.Arrange({0.0f, 0.0f, 100.0f, 40.0f});
        view.RemoveChild(content);
        EXPECT_EQ(view.GetContent(), nullptr);
        view.SetScroll(100.0f, 100.0f);
        view.Arrange(view.GetBounds());
        EXPECT_FLOAT_EQ(view.GetContentHeight(), 0.0f);
        EXPECT_FLOAT_EQ(view.GetScrollY(), 0.0f);
        UIElement *replacement = view.AddChild(std::make_unique<ResponsiveContent>());
        EXPECT_EQ(view.GetContent(), replacement);
        view.Arrange(view.GetBounds());
        view.ClearChildren();
        EXPECT_EQ(view.GetContent(), nullptr);
        view.Arrange(view.GetBounds());
        EXPECT_FLOAT_EQ(view.GetContentWidth(), 0.0f);
    }

    TEST(UIScrollTest, HorizontalAndDisabledAxesKeepOnlyPermittedScroll)
    {
        ScrollView view;
        auto       content = std::make_unique<UIElement>();
        content->SetLayout(Fixed(200.0f, 100.0f));
        view.SetContent(std::move(content));
        view.SetScrollAxes(ScrollAxes::Horizontal);
        UIPointerEvent wheel;
        wheel.action     = UIPointerAction::WHEEL;
        wheel.wheelDelta = 15.0f;
        EXPECT_EQ(view.OnPointerEvent(wheel), UIEventResult::HANDLED);
        view.Arrange({0.0f, 0.0f, 50.0f, 40.0f});
        EXPECT_FLOAT_EQ(view.GetScrollX(), 15.0f);
        EXPECT_FLOAT_EQ(view.GetScrollY(), 0.0f);
        EXPECT_FLOAT_EQ(view.GetContentHeight(), 40.0f);
        view.SetScrollAxes(ScrollAxes::None);
        EXPECT_EQ(view.OnPointerEvent(wheel), UIEventResult::UNHANDLED);
        view.Arrange(view.GetBounds());
        EXPECT_FLOAT_EQ(view.GetScrollX(), 0.0f);
        EXPECT_FLOAT_EQ(view.GetContentWidth(), 50.0f);
    }

    TEST(UIScrollTest, BoundedListMeasurementKeepsFullExtentAndDoesNotBuildRows)
    {
        ListView list;
        list.SetItemCount(1000);
        list.SetItemHeight(10.0f);
        int creations = 0;
        list.SetItemFactory([&](size_t) {
            ++creations;
            auto row = std::make_unique<UIElement>();
            row->SetLayout(Fixed(999.0f, 999.0f));
            return row;
        });
        EXPECT_FLOAT_EQ(list.MeasureDesired({{}, {100.0f, 50.0f}}).height, 50.0f);
        EXPECT_EQ(creations, 0);
        EXPECT_EQ(list.GetLiveItemCount(), 0u);
        EXPECT_FLOAT_EQ(list.GetContentHeight(), 10000.0f);
        list.Arrange({0.0f, 0.0f, 100.0f, 50.0f});
        ASSERT_GT(list.GetLiveItemCount(), 0u);
        EXPECT_LE(creations, 7);
        for (const auto &row : list.GetChildren()) {
            EXPECT_FLOAT_EQ(row->GetBounds().Width(), 100.0f);
            EXPECT_FLOAT_EQ(row->GetBounds().Height(), 10.0f);
            EXPECT_FALSE(row->IsLayoutDirty());
        }
        list.RemoveChild(list.GetChildren().front().get());
        list.Arrange(list.GetBounds());
        EXPECT_TRUE(list.IsItemLive(0));
        list.ClearChildren();
        EXPECT_EQ(list.GetLiveItemCount(), 0u);
        list.Arrange(list.GetBounds());
        EXPECT_TRUE(list.IsItemLive(0));
        list.Arrange({0.0f, 0.0f, 100.0f, 0.0f});
        EXPECT_EQ(list.GetLiveItemCount(), 0u);
    }

    TEST(UIScrollTest, DocumentAndBuilderShareScrollAndWrapProperties)
    {
        auto registry = UIElementRegistry::CreateDefault();
        auto document = UIDocumentLoader::LoadFromString(
            R"({"root":{"type":"ScrollView","props":{"scroll":{"axes":"vertical"}},"children":[{"type":"Text","props":{"text":{"wrap":true}}}]}})",
            registry, nullptr);
        ASSERT_NE(document->root, nullptr);
        ASSERT_FALSE(document->HasErrors());
        auto *loaded = static_cast<ScrollView *>(document->root.get());
        ASSERT_NE(loaded->GetContent(), nullptr);
        EXPECT_EQ(loaded->GetScrollAxes(), ScrollAxes::Vertical);
        EXPECT_TRUE(static_cast<Text *>(loaded->GetContent())->GetWrap());
        UIBuilder  builder(registry);
        UIElement *root = builder.SetRoot("ScrollView");
        ASSERT_TRUE(root->SetProperty("scroll.axes", UIPropertyValue::String("vertical")));
        UIElement *text = builder.AddChild(root, "Text");
        ASSERT_TRUE(text->SetProperty("text.wrap", UIPropertyValue::Bool(true)));
        auto built = builder.Build();
        EXPECT_EQ(static_cast<ScrollView *>(built.get())->GetScrollAxes(), loaded->GetScrollAxes());
        EXPECT_TRUE(static_cast<Text *>(static_cast<ScrollView *>(built.get())->GetContent())->GetWrap());
        EXPECT_FALSE(root->SetProperty("scroll.axes", UIPropertyValue::String("invalid")));
    }

} // namespace sky::ui
