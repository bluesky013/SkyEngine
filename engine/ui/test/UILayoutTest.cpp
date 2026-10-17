//
// Created on 2026/09/19.
//

#include <gtest/gtest.h>
#include <ui/UIContext.h>
#include <ui/UIElement.h>
#include <ui/UIEventRouter.h>
#include <ui/UILayout.h>
#include <ui/UIPaintContext.h>
#include <ui/widgets/DockPanel.h>
#include <ui/widgets/GridPanel.h>
#include <ui/widgets/HBox.h>
#include <ui/widgets/Overlay.h>
#include <ui/widgets/StackPanel.h>
#include <ui/widgets/VBox.h>

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

        UILayoutParams Fill()
        {
            UILayoutParams params;
            params.width.mode  = UISizeMode::FILL;
            params.height.mode = UISizeMode::FILL;
            return params;
        }

        class LayoutProbe : public UIElement {
        public:
            explicit LayoutProbe(float width = 10.0f, float height = 10.0f) : preferredWidth(width), preferredHeight(height)
            {
            }

            void Measure(float &width, float &height) override
            {
                ++measureCount;
                width  = preferredWidth;
                height = preferredHeight;
            }

            int arrangeCount = 0;
            int measureCount = 0;

        protected:
            void ArrangeChildren(const UIRect &content) override
            {
                ++arrangeCount;
                UIElement::ArrangeChildren(content);
            }

        private:
            float preferredWidth;
            float preferredHeight;
        };

        class PointerProbe : public LayoutProbe {
        public:
            UIEventResult OnPointerEvent(const UIPointerEvent &) override
            {
                return UIEventResult::HANDLED;
            }
        };

        void ExpectRect(const UIRect &actual, const UIRect &expected)
        {
            EXPECT_FLOAT_EQ(actual.left, expected.left);
            EXPECT_FLOAT_EQ(actual.top, expected.top);
            EXPECT_FLOAT_EQ(actual.right, expected.right);
            EXPECT_FLOAT_EQ(actual.bottom, expected.bottom);
        }

    } // namespace

    TEST(UILayoutTest, PercentWidthResolvesAgainstParent)
    {
        UIContext context;
        context.SetContentSize(200.0f, 200.0f);

        UIElement     *child = context.AddChild(std::make_unique<UIElement>());
        UILayoutParams params;
        params.width.mode   = UISizeMode::PERCENT;
        params.width.value  = 0.5f;
        params.height.mode  = UISizeMode::FIXED;
        params.height.value = 10.0f;
        child->SetLayout(params);

        context.Layout();
        EXPECT_FLOAT_EQ(child->GetBounds().Width(), 100.0f);
    }

    TEST(UILayoutTest, ZOrderControlsPaintOrder)
    {
        UIContext context;
        context.SetContentSize(100.0f, 100.0f);

        auto           firstHolder = std::make_unique<UIElement>();
        auto          *first       = firstHolder.get();
        UILayoutParams firstParams = Fixed(10.0f, 10.0f);
        firstParams.z              = 5.0f;
        first->SetLayout(firstParams);

        auto           secondHolder = std::make_unique<UIElement>();
        auto          *second       = secondHolder.get();
        UILayoutParams secondParams = Fixed(10.0f, 10.0f);
        secondParams.z              = 1.0f;
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

        auto  boxHolder = std::make_unique<HBox>();
        auto *box       = boxHolder.get();
        context.AddChild(std::move(boxHolder));
        box->SetLayout(Fixed(100.0f, 20.0f));
        box->SetSpacing(5.0f);

        auto  firstHolder = std::make_unique<UIElement>();
        auto *first       = firstHolder.get();
        first->SetLayout(Fixed(10.0f, 10.0f));
        box->AddChild(std::move(firstHolder));

        auto  secondHolder = std::make_unique<UIElement>();
        auto *second       = secondHolder.get();
        second->SetLayout(Fixed(10.0f, 10.0f));
        box->AddChild(std::move(secondHolder));

        context.Layout();

        EXPECT_FLOAT_EQ(first->GetBounds().left, 0.0f);
        EXPECT_FLOAT_EQ(first->GetBounds().right, 10.0f);
        EXPECT_FLOAT_EQ(second->GetBounds().left, 15.0f);
    }

    TEST(UILayoutTest, VBoxMeasuredHeightCoversChildren)
    {
        VBox           box;
        UILayoutParams params;
        params.paddingTop    = 2.0f;
        params.paddingBottom = 2.0f;
        box.SetLayout(params);
        box.SetSpacing(5.0f);

        box.AddChild(std::make_unique<UIElement>())->SetLayout(Fixed(10.0f, 10.0f));
        box.AddChild(std::make_unique<UIElement>())->SetLayout(Fixed(10.0f, 10.0f));

        float width  = 0.0f;
        float height = 0.0f;
        box.Measure(width, height);

        EXPECT_FLOAT_EQ(height, 29.0f);
    }

    TEST(UILayoutTest, ArrangeAcceptsFinalRectAndRecursesBeforePaint)
    {
        LayoutProbe    parent;
        UILayoutParams params = Fixed(1.0f, 1.0f);
        params.offsetLeft     = 100.0f;
        params.paddingLeft    = 3.0f;
        params.paddingTop     = 4.0f;
        params.paddingRight   = 5.0f;
        params.paddingBottom  = 6.0f;
        parent.SetLayout(params);
        UIElement *child = parent.AddChild(std::make_unique<UIElement>());
        child->SetLayout(Fill());

        parent.Arrange({10.0f, 20.0f, 110.0f, 80.0f});

        ExpectRect(parent.GetBounds(), {10.0f, 20.0f, 110.0f, 80.0f});
        ExpectRect(child->GetBounds(), {13.0f, 24.0f, 105.0f, 74.0f});
        EXPECT_EQ(parent.measureCount, 0);
        EXPECT_EQ(parent.arrangeCount, 1);
        EXPECT_FALSE(parent.IsLayoutDirty());
        EXPECT_FALSE(child->IsLayoutDirty());

        // Existing Layout callers still resolve their own legacy size rules.
        parent.Layout({0.0f, 0.0f, 200.0f, 100.0f});
        ExpectRect(parent.GetBounds(), {100.0f, 0.0f, 101.0f, 1.0f});
        EXPECT_EQ(parent.measureCount, 1);
    }

    TEST(UILayoutTest, NestedSlotContainersArrangeBeforeHitTestAndPaint)
    {
        UIContext context;
        context.SetContentSize(200.0f, 120.0f);
        auto       dockHolder = std::make_unique<DockPanel>();
        DockPanel *dock       = dockHolder.get();
        dock->SetLayout(Fill());
        auto        stackHolder = std::make_unique<StackPanel>();
        StackPanel *stack       = stackHolder.get();
        stack->SetPadding(2.0f);
        stack->SetGap(3.0f);
        auto         firstHolder = std::make_unique<LayoutProbe>();
        LayoutProbe *first       = firstHolder.get();
        StackSlot    firstSlot;
        firstSlot.width  = StackSize::Fixed;
        firstSlot.fixedW = 30.0f;
        firstSlot.height = StackSize::Fill;
        firstSlot.margin = 1.0f;
        stack->Add(std::move(firstHolder), firstSlot);
        StackSlot fillSlot;
        fillSlot.width    = StackSize::Fill;
        fillSlot.height   = StackSize::Fill;
        fillSlot.margin   = 2.0f;
        UIElement *second = stack->Add(std::make_unique<UIElement>(), fillSlot);
        dock->Add(std::move(stackHolder), {DockSide::Top, 24.0f});

        auto       overlayHolder = std::make_unique<Overlay>();
        Overlay   *overlay       = overlayHolder.get();
        auto       gridHolder    = std::make_unique<GridPanel>();
        GridPanel *grid          = gridHolder.get();
        grid->SetGrid(2, 2);
        auto         leafHolder = std::make_unique<LayoutProbe>();
        LayoutProbe *leaf       = leafHolder.get();
        grid->Add(std::move(leafHolder), {1, 1, 1, 1, 2.0f});
        overlay->AddChild(std::move(gridHolder));
        dock->Add(std::move(overlayHolder));
        context.AddChild(std::move(dockHolder));

        context.Layout();

        ExpectRect(first->GetBounds(), {3.0f, 3.0f, 33.0f, 21.0f});
        ExpectRect(second->GetBounds(), {39.0f, 4.0f, 196.0f, 20.0f});
        ExpectRect(overlay->GetBounds(), {0.0f, 24.0f, 200.0f, 120.0f});
        ExpectRect(grid->GetBounds(), overlay->GetBounds());
        ExpectRect(leaf->GetBounds(), {102.0f, 74.0f, 198.0f, 118.0f});
        UIEventRouter router(context);
        EXPECT_EQ(router.HitTest(150.0f, 90.0f), leaf);
        context.Traverse([](UIElement *element) { EXPECT_FALSE(element->IsLayoutDirty()); });

        UIPaintContext paint;
        context.Paint(paint);
        context.Paint(paint);

        EXPECT_EQ(first->arrangeCount, 1);
        EXPECT_EQ(leaf->arrangeCount, 1);
        ExpectRect(second->GetBounds(), {39.0f, 4.0f, 196.0f, 20.0f});
        context.Traverse([](UIElement *element) { EXPECT_FALSE(element->IsLayoutDirty()); });
    }

    TEST(UILayoutTest, StackMarginsPreserveAutoSizeAndSplitFillRemainder)
    {
        StackPanel stack(StackOrientation::Vertical);
        stack.SetPadding(2.0f);
        stack.SetGap(2.0f);
        StackSlot autoSlot;
        autoSlot.width   = StackSize::Fill;
        autoSlot.margin  = 3.0f;
        UIElement *first = stack.Add(std::make_unique<LayoutProbe>(10.0f, 12.0f), autoSlot);
        StackSlot  fillSlot;
        fillSlot.width    = StackSize::Fill;
        fillSlot.height   = StackSize::Fill;
        fillSlot.margin   = 1.0f;
        UIElement *second = stack.Add(std::make_unique<UIElement>(), fillSlot);
        fillSlot.weight   = 2.0f;
        UIElement *third  = stack.Add(std::make_unique<UIElement>(), fillSlot);

        stack.Arrange({0.0f, 0.0f, 80.0f, 90.0f});

        ExpectRect(first->GetBounds(), {5.0f, 5.0f, 75.0f, 17.0f});
        ExpectRect(second->GetBounds(), {3.0f, 23.0f, 77.0f, 43.0f});
        ExpectRect(third->GetBounds(), {3.0f, 47.0f, 77.0f, 87.0f});
    }

    TEST(UILayoutTest, SlotMetadataFollowsLiveChildren)
    {
        StackPanel stack;
        StackSlot  slot;
        slot.width        = StackSize::Fixed;
        slot.height       = StackSize::Fill;
        slot.fixedW       = 20.0f;
        UIElement *first  = stack.Add(std::make_unique<UIElement>(), slot);
        slot.fixedW       = 30.0f;
        UIElement *second = stack.Add(std::make_unique<UIElement>(), slot);
        ASSERT_TRUE(stack.MoveChild(second, -1));
        stack.Arrange({0.0f, 0.0f, 100.0f, 20.0f});
        ExpectRect(second->GetBounds(), {0.0f, 0.0f, 30.0f, 20.0f});
        ExpectRect(first->GetBounds(), {30.0f, 0.0f, 50.0f, 20.0f});

        auto removed = stack.RemoveChild(second);
        removed.reset();
        stack.Arrange(stack.GetBounds());
        ExpectRect(first->GetBounds(), {0.0f, 0.0f, 20.0f, 20.0f});
        stack.ClearChildren();
        UIElement *defaultSlot = stack.AddChild(std::make_unique<LayoutProbe>(7.0f, 8.0f));
        stack.Layout();
        ExpectRect(defaultSlot->GetBounds(), {0.0f, 0.0f, 7.0f, 8.0f});
        float width  = 0.0f;
        float height = 0.0f;
        stack.Measure(width, height);
        EXPECT_FLOAT_EQ(width, 7.0f);
        EXPECT_FLOAT_EQ(height, 8.0f);

        DockPanel  dock;
        UIElement *oldDock = dock.Add(std::make_unique<UIElement>(), {DockSide::Top, 50.0f});
        dock.RemoveChild(oldDock);
        UIElement *fill = dock.AddChild(std::make_unique<UIElement>());
        dock.Arrange({0.0f, 0.0f, 100.0f, 60.0f});
        ExpectRect(fill->GetBounds(), dock.GetBounds());
        dock.ClearChildren();
        fill = dock.AddChild(std::make_unique<UIElement>());
        dock.Layout();
        ExpectRect(fill->GetBounds(), dock.GetBounds());

        GridPanel grid;
        grid.SetGrid(2, 2);
        grid.Add(std::make_unique<UIElement>(), {1, 1});
        grid.ClearChildren();
        UIElement *cell = grid.AddChild(std::make_unique<UIElement>());
        grid.Arrange({0.0f, 0.0f, 100.0f, 60.0f});
        ExpectRect(cell->GetBounds(), {0.0f, 0.0f, 50.0f, 30.0f});
    }

    TEST(UILayoutTest, RemovingChildrenReleasesContainerPointerCapture)
    {
        auto verify = [](auto &container) {
            UIPointerEvent down;
            down.action       = UIPointerAction::DOWN;
            down.x            = 1.0f;
            down.y            = 1.0f;
            UIPointerEvent up = down;
            up.action         = UIPointerAction::UP;
            UIElement *child  = container.AddChild(std::make_unique<PointerProbe>());
            container.Arrange({0.0f, 0.0f, 100.0f, 60.0f});
            EXPECT_EQ(container.RoutePointer(down), UIEventResult::HANDLED);
            container.RemoveChild(child);
            EXPECT_EQ(container.RoutePointer(up), UIEventResult::UNHANDLED);

            container.AddChild(std::make_unique<PointerProbe>());
            container.Arrange(container.GetBounds());
            EXPECT_EQ(container.RoutePointer(down), UIEventResult::HANDLED);
            container.ClearChildren();
            EXPECT_EQ(container.RoutePointer(up), UIEventResult::UNHANDLED);
        };
        StackPanel stack;
        DockPanel  dock;
        GridPanel  grid;
        Overlay    overlay;
        verify(stack);
        verify(dock);
        verify(grid);
        verify(overlay);
    }

} // namespace sky::ui
