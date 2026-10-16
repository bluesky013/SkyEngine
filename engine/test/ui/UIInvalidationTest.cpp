//
// Incremental layout retains clean branches while preserving legacy polling.
//

#include <gtest/gtest.h>
#include <ui/IUITextureRegistry.h>
#include <ui/UIContext.h>
#include <ui/UIPaintContext.h>
#include <ui/localization/UILocalization.h>
#include <ui/text/UIBuiltinFontProvider.h>
#include <ui/text/UITextSystem.h>
#include <ui/widgets/ListView.h>
#include <ui/widgets/ScrollView.h>
#include <ui/widgets/Text.h>
#include <ui/widgets/VBox.h>

#include <limits>

namespace sky::ui {

    namespace {

        UILayoutParams FillLayout()
        {
            UILayoutParams layout;
            layout.width.mode  = UISizeMode::FILL;
            layout.height.mode = UISizeMode::FILL;
            return layout;
        }

        UILayoutParams FixedLayout(float width, float height, float left = 0.0f, float top = 0.0f)
        {
            UILayoutParams layout;
            layout.width      = {UISizeMode::FIXED, width};
            layout.height     = {UISizeMode::FIXED, height};
            layout.offsetLeft = left;
            layout.offsetTop  = top;
            return layout;
        }

        class RetainedProbe : public UIElement {
        public:
            RetainedProbe()
            {
                SetLayoutVolatile(false);
            }

            void SetNaturalSize(UISize value)
            {
                natural = value;
                MarkLayoutDirty();
            }

            void ResetCounters()
            {
                measures = arrangements = paints = 0;
            }

            void OnPaint(UIPaintContext &context) override
            {
                ++paints;
                context.AddRect(GetBounds(), 0xFFFFFFFFu);
            }

            UISize natural{24.0f, 10.0f};
            int    measures                = 0;
            int    arrangements            = 0;
            int    paints                  = 0;
            bool   invalidateDuringArrange = false;
            bool   invalidateDuringMeasure = false;
            bool   addChildDuringArrange   = false;

        protected:
            UISize OnMeasure(const UIConstraints &) override
            {
                ++measures;
                const UISize measured = natural;
                if (invalidateDuringMeasure) {
                    invalidateDuringMeasure = false;
                    natural.width           = 48.0f;
                    MarkLayoutDirty();
                }
                return measured;
            }

            void ArrangeChildren(const UIRect &content) override
            {
                ++arrangements;
                if (invalidateDuringArrange) {
                    invalidateDuringArrange = false;
                    MarkLayoutDirty();
                }
                if (addChildDuringArrange) {
                    addChildDuringArrange = false;
                    auto child            = std::make_unique<RetainedProbe>();
                    child->SetLayout(FillLayout());
                    AddChild(std::move(child));
                }
                UIElement::ArrangeChildren(content);
            }
        };

        class LegacyProbe : public UIElement {
        public:
            void Measure(float &width, float &height) override
            {
                ++measures;
                width  = natural.width;
                height = natural.height;
            }

            UISize natural{20.0f, 10.0f};
            int    measures = 0;
        };

        class CountingText : public Text {
        public:
            int measures = 0;

        protected:
            UISize OnMeasure(const UIConstraints &constraints) override
            {
                ++measures;
                return Text::OnMeasure(constraints);
            }
        };

        class FontTextureRegistry : public IUITextureRegistry {
        public:
            UITextureId RegisterTexture(const UIImageData &) override
            {
                return nextId++;
            }

            UITextureId RegisterTextureAs(UITextureId id, const UIImageData &) override
            {
                return id;
            }

            void UpdateTexture(UITextureId, const UIImageData &) override
            {
            }
            void ReleaseTexture(UITextureId) override
            {
            }

        private:
            UITextureId nextId = 1;
        };

        void ExpectRect(const UIRect &actual, const UIRect &expected)
        {
            EXPECT_FLOAT_EQ(actual.left, expected.left);
            EXPECT_FLOAT_EQ(actual.top, expected.top);
            EXPECT_FLOAT_EQ(actual.right, expected.right);
            EXPECT_FLOAT_EQ(actual.bottom, expected.bottom);
        }

    } // namespace

    TEST(UIInvalidationTest, CleanRetainedTreeDoesNoLayoutWorkOnIdleFrames)
    {
        UIContext context;
        context.SetContentSize(200.0f, 100.0f);
        auto  parentOwner = std::make_unique<RetainedProbe>();
        auto *parent      = parentOwner.get();
        parent->SetLayout(FillLayout());
        auto  leafOwner = std::make_unique<RetainedProbe>();
        auto *leaf      = leafOwner.get();
        leaf->SetLayout(FillLayout());
        parent->AddChild(std::move(leafOwner));
        context.AddChild(std::move(parentOwner));
        context.Layout();
        ASSERT_FALSE(parent->IsLayoutDirty());
        ASSERT_FALSE(leaf->IsLayoutDirty());
        parent->ResetCounters();
        leaf->ResetCounters();

        for (int frame = 0; frame < 5; ++frame) {
            context.Layout();
        }

        EXPECT_EQ(parent->measures, 0);
        EXPECT_EQ(parent->arrangements, 0);
        EXPECT_EQ(leaf->measures, 0);
        EXPECT_EQ(leaf->arrangements, 0);
        ExpectRect(leaf->GetBounds(), {0.0f, 0.0f, 200.0f, 100.0f});
    }

    TEST(UIInvalidationTest, IntrinsicLeafChangeResizesAutoParentAndSkipsUnchangedSibling)
    {
        UIContext context;
        context.SetContentSize(200.0f, 160.0f);
        auto  parentOwner = std::make_unique<VBox>();
        auto *parent      = parentOwner.get();
        auto  leafOwner   = std::make_unique<RetainedProbe>();
        auto *leaf        = leafOwner.get();
        leaf->SetNaturalSize({20.0f, 10.0f});
        parent->AddChild(std::move(leafOwner));
        context.AddChild(std::move(parentOwner));
        auto  siblingOwner = std::make_unique<RetainedProbe>();
        auto *sibling      = siblingOwner.get();
        sibling->SetLayout(FixedLayout(50.0f, 15.0f, 0.0f, 100.0f));
        context.AddChild(std::move(siblingOwner));
        context.Layout();
        sibling->ResetCounters();

        leaf->SetNaturalSize({60.0f, 25.0f});
        EXPECT_TRUE(leaf->IsLayoutDirty());
        EXPECT_TRUE(parent->IsLayoutDirty());
        EXPECT_TRUE(context.GetRoot()->IsLayoutDirty());
        EXPECT_FALSE(sibling->IsLayoutDirty());
        context.Layout();

        ExpectRect(parent->GetBounds(), {0.0f, 0.0f, 60.0f, 25.0f});
        ExpectRect(leaf->GetBounds(), parent->GetBounds());
        EXPECT_EQ(sibling->measures, 0);
        EXPECT_EQ(sibling->arrangements, 0);
        ExpectRect(sibling->GetBounds(), {0.0f, 100.0f, 50.0f, 115.0f});
    }

    TEST(UIInvalidationTest, ResizeUpdatesAssignedSlotsWithoutPredirtyingDescendants)
    {
        UIContext context;
        context.SetContentSize(100.0f, 80.0f);
        auto  parentOwner = std::make_unique<RetainedProbe>();
        auto *parent      = parentOwner.get();
        parent->SetLayout(FillLayout());
        auto  leafOwner = std::make_unique<RetainedProbe>();
        auto *leaf      = leafOwner.get();
        leaf->SetLayout(FillLayout());
        parent->AddChild(std::move(leafOwner));
        context.AddChild(std::move(parentOwner));
        context.Layout();
        parent->ResetCounters();
        leaf->ResetCounters();

        context.SetContentSize(300.0f, 200.0f);
        EXPECT_FALSE(parent->IsLayoutDirty());
        EXPECT_FALSE(leaf->IsLayoutDirty());
        context.Layout();

        ExpectRect(parent->GetBounds(), {0.0f, 0.0f, 300.0f, 200.0f});
        ExpectRect(leaf->GetBounds(), parent->GetBounds());
        EXPECT_EQ(parent->arrangements, 1);
        EXPECT_EQ(leaf->arrangements, 1);
        EXPECT_FALSE(context.GetRoot()->IsLayoutDirty());
    }

    TEST(UIInvalidationTest, MeasurementCachesSeveralNormalizedBudgetsWithoutChangingLayoutState)
    {
        RetainedProbe probe;
        probe.SetNaturalSize({500.0f, 300.0f});
        const UIConstraints narrow{{}, {100.0f, 100.0f}};
        const UIConstraints wide{{}, {200.0f, 100.0f}};
        const UIConstraints tall{{}, {100.0f, 200.0f}};
        const UIRect        bounds = probe.GetBounds();
        const bool          dirty  = probe.IsLayoutDirty();

        EXPECT_FLOAT_EQ(probe.MeasureDesired(narrow).width, 100.0f);
        EXPECT_FLOAT_EQ(probe.MeasureDesired(wide).width, 200.0f);
        EXPECT_FLOAT_EQ(probe.MeasureDesired(tall).height, 200.0f);
        EXPECT_EQ(probe.measures, 3);
        const float nan = std::numeric_limits<float>::quiet_NaN();
        EXPECT_FLOAT_EQ(probe.MeasureDesired({{-1.0f, nan}, {100.0f, 100.0f}}).width, 100.0f);
        EXPECT_FLOAT_EQ(probe.MeasureDesired(wide).width, 200.0f);
        EXPECT_FLOAT_EQ(probe.MeasureDesired(tall).height, 200.0f);

        EXPECT_EQ(probe.measures, 3);
        ExpectRect(probe.GetBounds(), bounds);
        EXPECT_EQ(probe.IsLayoutDirty(), dirty);
        EXPECT_TRUE(probe.GetChildren().empty());
    }

    TEST(UIInvalidationTest, RepeatedMutationWhileAlreadyDirtyInvalidatesOwnAndAncestorMeasurements)
    {
        VBox  parent;
        auto  leafOwner = std::make_unique<RetainedProbe>();
        auto *leaf      = leafOwner.get();
        leaf->SetNaturalSize({10.0f, 10.0f});
        parent.AddChild(std::move(leafOwner));
        const UIConstraints budget{{}, {200.0f, 200.0f}};

        EXPECT_FLOAT_EQ(parent.MeasureDesired(budget).width, 10.0f);
        ASSERT_TRUE(parent.IsLayoutDirty());
        ASSERT_TRUE(leaf->IsLayoutDirty());
        leaf->SetNaturalSize({40.0f, 30.0f});
        const UISize second = parent.MeasureDesired(budget);
        EXPECT_FLOAT_EQ(second.width, 40.0f);
        EXPECT_FLOAT_EQ(second.height, 30.0f);
        leaf->SetNaturalSize({70.0f, 35.0f});
        const UISize third = parent.MeasureDesired(budget);
        EXPECT_FLOAT_EQ(third.width, 70.0f);
        EXPECT_FLOAT_EQ(third.height, 35.0f);
        EXPECT_TRUE(parent.IsLayoutDirty());
    }

    TEST(UIInvalidationTest, MeasurementCacheEvictsOldBudgetsAfterFourEntries)
    {
        RetainedProbe probe;
        probe.SetNaturalSize({500.0f, 300.0f});
        const UIConstraints oldest{{}, {100.0f, 100.0f}};
        for (int index = 0; index < 5; ++index) {
            const float width = 100.0f + index * 10.0f;
            EXPECT_FLOAT_EQ(probe.MeasureDesired({{}, {width, 100.0f}}).width, width);
        }
        ASSERT_EQ(probe.measures, 5);

        EXPECT_FLOAT_EQ(probe.MeasureDesired(oldest).width, 100.0f);
        EXPECT_EQ(probe.measures, 6);
        probe.MeasureDesired(oldest);
        EXPECT_EQ(probe.measures, 6);
    }

    TEST(UIInvalidationTest, ArrangeOnlyInvalidationPreservesMeasurementCache)
    {
        RetainedProbe       probe;
        const UIConstraints budget{{}, {100.0f, 100.0f}};
        probe.MeasureDesired(budget);
        probe.Arrange({0.0f, 0.0f, 24.0f, 10.0f});
        const int measured = probe.measures;
        probe.MarkArrangeDirty();
        EXPECT_TRUE(probe.IsLayoutDirty());
        probe.MeasureDesired(budget);
        EXPECT_EQ(probe.measures, measured);
        probe.MarkLayoutDirty();
        probe.MeasureDesired(budget);
        EXPECT_EQ(probe.measures, measured + 1);
    }

    TEST(UIInvalidationTest, ScrollOffsetRearrangesContentWithoutRemeasuringIt)
    {
        UIContext context;
        context.SetContentSize(100.0f, 30.0f);
        auto  scrollOwner = std::make_unique<ScrollView>();
        auto *scroll      = scrollOwner.get();
        scroll->SetScrollAxes(ScrollAxes::Vertical);
        scroll->SetLayout(FillLayout());
        auto  contentOwner = std::make_unique<RetainedProbe>();
        auto *content      = contentOwner.get();
        content->SetNaturalSize({100.0f, 120.0f});
        scroll->SetContent(std::move(contentOwner));
        context.AddChild(std::move(scrollOwner));
        context.Layout();
        content->ResetCounters();

        scroll->SetScroll(0.0f, 40.0f);
        EXPECT_FALSE(content->IsLayoutDirty());
        context.Layout();

        EXPECT_EQ(content->measures, 0);
        EXPECT_EQ(content->arrangements, 1);
        EXPECT_FLOAT_EQ(scroll->GetContentHeight(), 120.0f);
        ExpectRect(content->GetBounds(), {0.0f, -40.0f, 100.0f, 80.0f});
    }

    TEST(UIInvalidationTest, LegacyWidgetsRemainVolatileAndObserveExternalSizeChanges)
    {
        UIContext context;
        context.SetContentSize(200.0f, 100.0f);
        auto  owner  = std::make_unique<LegacyProbe>();
        auto *legacy = owner.get();
        ASSERT_TRUE(legacy->IsLayoutVolatile());
        context.AddChild(std::move(owner));
        context.Layout();
        legacy->measures = 0;

        legacy->natural.width = 55.0f;
        context.Layout();

        EXPECT_GT(legacy->measures, 0);
        EXPECT_FLOAT_EQ(legacy->GetBounds().Width(), 55.0f);
        legacy->measures = 0;
        context.Layout();
        EXPECT_GT(legacy->measures, 0);
    }

    TEST(UIInvalidationTest, ExplicitLayoutAndArrangeForceCleanSubtreeTraversal)
    {
        RetainedProbe parent;
        parent.SetLayout(FillLayout());
        auto  childOwner = std::make_unique<RetainedProbe>();
        auto *child      = childOwner.get();
        child->SetLayout(FillLayout());
        parent.AddChild(std::move(childOwner));
        const UIRect slot{0.0f, 0.0f, 100.0f, 80.0f};
        parent.Arrange(slot);
        parent.ResetCounters();
        child->ResetCounters();

        parent.Arrange(slot);
        parent.Layout(slot);

        EXPECT_EQ(parent.arrangements, 2);
        EXPECT_EQ(child->arrangements, 2);
        ExpectRect(child->GetBounds(), slot);
    }

    TEST(UIInvalidationTest, ConditionalEntryPointsSkipOnlyWhenTheirInputsStayEqual)
    {
        RetainedProbe probe;
        probe.SetLayout(FillLayout());
        const UIRect first{0.0f, 0.0f, 100.0f, 80.0f};
        probe.LayoutIfNeeded(first);
        probe.ResetCounters();
        probe.LayoutIfNeeded(first);
        EXPECT_EQ(probe.measures, 0);
        EXPECT_EQ(probe.arrangements, 0);

        probe.LayoutIfNeeded({10.0f, 20.0f, 110.0f, 100.0f});
        EXPECT_EQ(probe.arrangements, 1);
        ExpectRect(probe.GetBounds(), {10.0f, 20.0f, 110.0f, 100.0f});
        probe.ResetCounters();
        probe.ArrangeIfNeeded(probe.GetBounds());
        EXPECT_EQ(probe.arrangements, 0);
        probe.ArrangeIfNeeded({10.0f, 20.0f, 210.0f, 120.0f});
        EXPECT_EQ(probe.arrangements, 1);
        ExpectRect(probe.GetBounds(), {10.0f, 20.0f, 210.0f, 120.0f});
    }

    TEST(UIInvalidationTest, PaintAlwaysRebuildsCompleteDrawDataAndVisualChangesDoNoLayoutWork)
    {
        UIContext context;
        context.SetContentSize(100.0f, 50.0f);
        auto  firstOwner  = std::make_unique<RetainedProbe>();
        auto *first       = firstOwner.get();
        auto  secondOwner = std::make_unique<RetainedProbe>();
        auto *second      = secondOwner.get();
        second->SetLayout(FixedLayout(24.0f, 10.0f, 30.0f));
        context.AddChild(std::move(firstOwner));
        context.AddChild(std::move(secondOwner));
        context.Layout();
        first->ResetCounters();
        second->ResetCounters();
        UIPaintContext paint;
        context.Paint(paint);
        ASSERT_EQ(paint.GetDrawData().vertices.size(), 8u);
        context.Paint(paint);
        EXPECT_EQ(paint.GetDrawData().vertices.size(), 8u);
        EXPECT_EQ(first->paints, 2);
        EXPECT_EQ(second->paints, 2);

        UITransform transform;
        transform.translationX = 4.0f;
        first->SetTransform(transform);
        first->SetOpacity(0.5f);
        EXPECT_FALSE(first->IsLayoutDirty());
        EXPECT_FALSE(context.GetRoot()->IsLayoutDirty());
        context.Layout();
        EXPECT_EQ(first->measures, 0);
        EXPECT_EQ(first->arrangements, 0);
        EXPECT_EQ(second->measures, 0);
        EXPECT_EQ(second->arrangements, 0);
        context.Paint(paint);
        ASSERT_EQ(paint.GetDrawData().vertices.size(), 8u);
        EXPECT_FLOAT_EQ(paint.GetDrawData().vertices.front().x, 4.0f);
        EXPECT_EQ(first->paints, 3);
        EXPECT_EQ(second->paints, 3);
    }

    TEST(UIInvalidationTest, InvalidationsRaisedDuringArrangeSurviveUntilFollowingPass)
    {
        UIContext context;
        context.SetContentSize(100.0f, 50.0f);
        auto  owner = std::make_unique<RetainedProbe>();
        auto *probe = owner.get();
        probe->SetLayout(FillLayout());
        probe->invalidateDuringArrange = true;
        context.AddChild(std::move(owner));

        context.Layout();
        EXPECT_TRUE(probe->IsLayoutDirty());
        EXPECT_TRUE(context.GetRoot()->IsLayoutDirty());
        probe->ResetCounters();
        context.Layout();
        EXPECT_EQ(probe->arrangements, 1);
        EXPECT_FALSE(probe->IsLayoutDirty());
        EXPECT_FALSE(context.GetRoot()->IsLayoutDirty());
        probe->ResetCounters();
        context.Layout();
        EXPECT_EQ(probe->measures, 0);
        EXPECT_EQ(probe->arrangements, 0);
    }

    TEST(UIInvalidationTest, ChildAddedDuringArrangePreservesAncestorInvalidationUntilSettled)
    {
        UIContext context;
        context.SetContentSize(100.0f, 50.0f);
        auto  owner  = std::make_unique<RetainedProbe>();
        auto *parent = owner.get();
        parent->SetLayout(FillLayout());
        parent->addChildDuringArrange = true;
        context.AddChild(std::move(owner));

        context.Layout();
        ASSERT_EQ(parent->GetChildren().size(), 1u);
        EXPECT_TRUE(parent->IsLayoutDirty());
        EXPECT_TRUE(context.GetRoot()->IsLayoutDirty());
        ExpectRect(parent->GetChildren().front()->GetBounds(), {0.0f, 0.0f, 100.0f, 50.0f});
        context.Layout();

        EXPECT_FALSE(parent->IsLayoutDirty());
        EXPECT_FALSE(parent->GetChildren().front()->IsLayoutDirty());
        EXPECT_FALSE(context.GetRoot()->IsLayoutDirty());
        parent->ResetCounters();
        context.Layout();
        EXPECT_EQ(parent->measures, 0);
        EXPECT_EQ(parent->arrangements, 0);
        EXPECT_EQ(parent->GetChildren().size(), 1u);
    }

    TEST(UIInvalidationTest, ListCreatesRowsForLateAndReplacedFactoriesOnlyDuringArrangement)
    {
        UIContext context;
        context.SetContentSize(100.0f, 20.0f);
        auto  owner = std::make_unique<ListView>();
        auto *list  = owner.get();
        list->SetLayout(FillLayout());
        list->SetItemCount(100);
        list->SetItemHeight(10.0f);
        context.AddChild(std::move(owner));
        context.Layout();
        ASSERT_EQ(list->GetLiveItemCount(), 0u);
        ASSERT_FALSE(list->IsLayoutDirty());
        int oldFactoryCalls = 0;
        list->SetItemFactory([&oldFactoryCalls](size_t index) {
            ++oldFactoryCalls;
            auto row = std::make_unique<RetainedProbe>();
            row->SetName("old-" + std::to_string(index));
            return row;
        });
        const UIConstraints budget{{}, {100.0f, 20.0f}};
        list->MeasureDesired(budget);
        EXPECT_EQ(oldFactoryCalls, 0);
        EXPECT_EQ(list->GetLiveItemCount(), 0u);
        context.Layout();
        ASSERT_GT(list->GetLiveItemCount(), 0u);
        EXPECT_EQ(static_cast<size_t>(oldFactoryCalls), list->GetLiveItemCount());
        EXPECT_LE(list->GetLiveItemCount(), 4u);
        context.Layout();
        ASSERT_FALSE(list->IsLayoutDirty());
        const int previousCalls = oldFactoryCalls;

        int newFactoryCalls = 0;
        list->SetItemFactory([&newFactoryCalls](size_t index) {
            ++newFactoryCalls;
            auto row = std::make_unique<RetainedProbe>();
            row->SetName("new-" + std::to_string(index));
            return row;
        });
        EXPECT_EQ(list->GetLiveItemCount(), 0u);
        EXPECT_TRUE(list->GetChildren().empty());
        list->MeasureDesired(budget);
        EXPECT_EQ(newFactoryCalls, 0);
        EXPECT_EQ(oldFactoryCalls, previousCalls);
        context.Layout();
        ASSERT_GT(list->GetLiveItemCount(), 0u);
        EXPECT_EQ(static_cast<size_t>(newFactoryCalls), list->GetLiveItemCount());
        for (const auto &row : list->GetChildren()) {
            EXPECT_EQ(row->GetName().find("new-"), 0u);
            EXPECT_FLOAT_EQ(row->GetBounds().Width(), 100.0f);
            EXPECT_FLOAT_EQ(row->GetBounds().Height(), 10.0f);
        }
        context.Layout();
        EXPECT_FALSE(list->IsLayoutDirty());
        const int settledCalls = newFactoryCalls;
        context.Layout();
        EXPECT_EQ(newFactoryCalls, settledCalls);
        EXPECT_EQ(oldFactoryCalls, previousCalls);
    }

    TEST(UIInvalidationTest, ReparentingAndVolatilityChangesUpdateOnlyTheLiveAncestorBranches)
    {
        UIContext context;
        context.SetContentSize(300.0f, 100.0f);
        auto  firstOwner = std::make_unique<RetainedProbe>();
        auto *first      = firstOwner.get();
        first->SetLayout(FixedLayout(100.0f, 100.0f));
        auto  secondOwner = std::make_unique<RetainedProbe>();
        auto *second      = secondOwner.get();
        second->SetLayout(FixedLayout(100.0f, 100.0f, 120.0f));
        context.AddChild(std::move(firstOwner));
        context.AddChild(std::move(secondOwner));
        context.Layout();

        auto  legacyOwner = std::make_unique<LegacyProbe>();
        auto *legacy      = legacyOwner.get();
        first->AddChild(std::move(legacyOwner));
        context.Layout();
        first->ResetCounters();
        second->ResetCounters();
        context.Layout();
        EXPECT_EQ(first->arrangements, 1);
        EXPECT_EQ(second->arrangements, 0);

        UIElementPtr detached = first->RemoveChild(legacy);
        ASSERT_NE(detached, nullptr);
        EXPECT_EQ(detached->GetParent(), nullptr);
        context.Layout();
        first->ResetCounters();
        second->ResetCounters();
        context.Layout();
        EXPECT_EQ(first->arrangements, 0);
        EXPECT_EQ(second->arrangements, 0);

        second->AddChild(std::move(detached));
        context.Layout();
        EXPECT_EQ(legacy->GetParent(), second);
        ExpectRect(legacy->GetBounds(), {120.0f, 0.0f, 140.0f, 10.0f});
        first->ResetCounters();
        second->ResetCounters();
        context.Layout();
        EXPECT_EQ(first->arrangements, 0);
        EXPECT_EQ(second->arrangements, 1);

        legacy->SetLayoutVolatile(false);
        context.Layout();
        second->ResetCounters();
        context.Layout();
        EXPECT_EQ(second->arrangements, 0);
        legacy->SetLayoutVolatile(true);
        context.Layout();
        EXPECT_EQ(second->arrangements, 1);
        second->ClearChildren();
        context.Layout();
        second->ResetCounters();
        context.Layout();
        EXPECT_EQ(second->arrangements, 0);
    }

    TEST(UIInvalidationTest, MutationDuringMeasureSurvivesTheFollowingArrange)
    {
        UIContext context;
        context.SetContentSize(100.0f, 50.0f);
        auto  owner                    = std::make_unique<RetainedProbe>();
        auto *probe                    = owner.get();
        probe->invalidateDuringMeasure = true;
        context.AddChild(std::move(owner));

        context.Layout();
        EXPECT_FLOAT_EQ(probe->GetBounds().Width(), 24.0f);
        EXPECT_TRUE(probe->IsLayoutDirty());
        EXPECT_TRUE(context.GetRoot()->IsLayoutDirty());
        context.Layout();

        EXPECT_FLOAT_EQ(probe->GetBounds().Width(), 48.0f);
        EXPECT_FALSE(probe->IsLayoutDirty());
        EXPECT_FALSE(context.GetRoot()->IsLayoutDirty());
        probe->ResetCounters();
        context.Layout();
        EXPECT_EQ(probe->measures, 0);
        EXPECT_EQ(probe->arrangements, 0);
    }

    TEST(UIInvalidationTest, ExplicitArrangeCannotReviveParentInputCachedBeforeLayoutMutation)
    {
        RetainedProbe probe;
        probe.SetNaturalSize({10.0f, 10.0f});
        const UIRect parent{0.0f, 0.0f, 100.0f, 100.0f};
        probe.LayoutIfNeeded(parent);
        const UIRect oldBounds = probe.GetBounds();
        ASSERT_FLOAT_EQ(oldBounds.Width(), 10.0f);

        probe.SetLayout(FixedLayout(20.0f, 10.0f));
        probe.Arrange(oldBounds);
        ASSERT_FALSE(probe.IsLayoutDirty());
        probe.LayoutIfNeeded(parent);

        EXPECT_FLOAT_EQ(probe.GetBounds().Width(), 20.0f);
        probe.ResetCounters();
        probe.LayoutIfNeeded(parent);
        EXPECT_EQ(probe.measures, 0);
        EXPECT_EQ(probe.arrangements, 0);

        // The input must also stay invalid when a measurement hook schedules
        // new work before a later forced Arrange consumes the current flag.
        RetainedProbe changing;
        changing.invalidateDuringMeasure = true;
        changing.LayoutIfNeeded(parent);
        ASSERT_TRUE(changing.IsLayoutDirty());
        changing.Arrange(changing.GetBounds());
        ASSERT_FALSE(changing.IsLayoutDirty());
        changing.LayoutIfNeeded(parent);
        EXPECT_FLOAT_EQ(changing.GetBounds().Width(), 48.0f);
    }

    TEST(UIInvalidationTest, AssigningResolvedBoundsDoesNotInvalidateCleanAncestors)
    {
        RetainedProbe parent;
        auto          childOwner = std::make_unique<RetainedProbe>();
        auto         *child      = childOwner.get();
        parent.AddChild(std::move(childOwner));
        parent.Arrange({0.0f, 0.0f, 100.0f, 100.0f});
        ASSERT_FALSE(parent.IsLayoutDirty());

        child->Arrange({20.0f, 30.0f, 50.0f, 40.0f});

        EXPECT_FALSE(parent.IsLayoutDirty());
        EXPECT_FALSE(child->IsLayoutDirty());
        ExpectRect(child->GetBounds(), {20.0f, 30.0f, 50.0f, 40.0f});
    }

    TEST(UIInvalidationTest, LocalizedTextKeepsPollingUntilTheKeyIsRemoved)
    {
        auto             *localization = UILocalization::Get();
        const std::string oldLocale    = localization->GetLocale();
        struct RestoreLocale {
            UILocalization *table;
            std::string     previous;
            ~RestoreLocale()
            {
                table->SetLocale(previous);
                table->ClearLocale("ui-invalidation-short");
                table->ClearLocale("ui-invalidation-long");
            }
        } restore{localization, oldLocale};
        localization->AddString("ui-invalidation-short", "size", "AB");
        localization->AddString("ui-invalidation-long", "size", "ABCDEFG");
        localization->SetLocale("ui-invalidation-short");
        FontTextureRegistry   registry;
        UIBuiltinFontProvider font;
        UITextSystem          textSystem(&font, &registry);
        UIContext             context;
        context.SetContentSize(200.0f, 100.0f);
        auto  owner = std::make_unique<CountingText>();
        auto *text  = owner.get();
        text->SetTextSystem(&textSystem);
        text->SetFontSize(10);
        text->SetTextKey("size");
        ASSERT_TRUE(text->IsLayoutVolatile());
        context.AddChild(std::move(owner));
        context.Layout();
        EXPECT_FLOAT_EQ(text->GetBounds().Width(), 12.0f);
        text->measures = 0;

        localization->SetLocale("ui-invalidation-long");
        context.Layout();
        EXPECT_GT(text->measures, 0);
        EXPECT_FLOAT_EQ(text->GetBounds().Width(), 42.0f);
        text->SetTextKey("");
        text->SetContent("X");
        EXPECT_FALSE(text->IsLayoutVolatile());
        context.Layout();
        EXPECT_FLOAT_EQ(text->GetBounds().Width(), 6.0f);
        text->measures = 0;
        context.Layout();
        EXPECT_EQ(text->measures, 0);
    }

} // namespace sky::ui
