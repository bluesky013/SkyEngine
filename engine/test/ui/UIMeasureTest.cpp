#include <gtest/gtest.h>
#include <ui/IUITextureRegistry.h>
#include <ui/UIContext.h>
#include <ui/UIPaintContext.h>
#include <ui/text/UITextSystem.h>
#include <ui/widgets/DockPanel.h>
#include <ui/widgets/GridPanel.h>
#include <ui/widgets/HBox.h>
#include <ui/widgets/ListView.h>
#include <ui/widgets/Overlay.h>
#include <ui/widgets/ScrollView.h>
#include <ui/widgets/StackPanel.h>
#include <ui/widgets/Text.h>
#include <ui/widgets/VBox.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace sky::ui {
    namespace {
        UIConstraints Budget(float width, float height = std::numeric_limits<float>::infinity())
        {
            return {{}, {width, height}};
        }

        class WrappingProbe : public UIElement {
        public:
            explicit WrappingProbe(float naturalWidth = 120.0f, float area = 240.0f) : naturalWidth(naturalWidth), area(area)
            {
            }
            UIConstraints lastConstraints;
            int           arrangeCount = 0;

        protected:
            UISize OnMeasure(const UIConstraints &constraints) override
            {
                lastConstraints   = constraints;
                const float width = constraints.Constrain({naturalWidth, 0.0f}).width;
                return {width, width > 0.0f ? std::ceil(area / width) * 10.0f : 0.0f};
            }
            void ArrangeChildren(const UIRect &) override
            {
                ++arrangeCount;
            }

        private:
            float naturalWidth;
            float area;
        };

        class LegacyProbe : public UIElement {
        public:
            LegacyProbe(float width = 30.0f, float height = 12.0f) : width(width), height(height)
            {
            }
            void Measure(float &outWidth, float &outHeight) override
            {
                outWidth  = width;
                outHeight = height;
            }

        private:
            float width;
            float height;
        };

        class ConstraintProbe : public UIElement {
        public:
            UISize        desired{30.0f, 12.0f};
            UIConstraints received;

        protected:
            UISize OnMeasure(const UIConstraints &constraints) override
            {
                received = constraints;
                return desired;
            }
        };

        class FractionalFont : public IUIFontProvider {
        public:
            bool GetGlyph(uint32_t, uint32_t, UIGlyphBitmap &out) override
            {
                out       = {};
                out.width = out.height = 1;
                out.advance            = 6.1f;
                out.bearingY           = 10.0f;
                out.pixels             = {255};
                return true;
            }
            UIFontMetrics GetFontMetrics(uint32_t) override
            {
                return {10.0f, 0.0f, 10.0f};
            }
        };

        class MeasurementTextures : public IUITextureRegistry {
        public:
            UITextureId RegisterTexture(const UIImageData &) override
            {
                return 1;
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
        };

        class AllocatedText : public Text {
        public:
            int arrangements = 0;

        protected:
            void ArrangeChildren(const UIRect &content) override
            {
                ++arrangements;
                UIElement::ArrangeChildren(content);
            }
        };

        void ExpectTextLines(Text &text, float height)
        {
            UIPaintContext context;
            context.Begin({-2000000.0f, 0.0f, 2000000.0f, 100.0f});
            text.Paint(context);
            const auto &vertices = context.GetDrawData().vertices;
            ASSERT_EQ(vertices.size(), 8u);
            EXPECT_FLOAT_EQ(vertices[0].y, 0.0f);
            EXPECT_FLOAT_EQ(vertices[4].y, height - 10.0f);
        }

        template <typename Base>
        class NativeMeasureSubclass : public Base {
        public:
            UISize desired{99.0f, 88.0f};
            int    queries = 0;

        protected:
            UISize OnMeasure(const UIConstraints &) override
            {
                ++queries;
                return desired;
            }
        };

        template <typename Base>
        void ExpectNativeMeasurementEntriesAgree()
        {
            NativeMeasureSubclass<Base> element;
            float                       width  = 0.0f;
            float                       height = 0.0f;
            UIElement                  &base   = element;
            base.Measure(width, height);
            EXPECT_FLOAT_EQ(width, 99.0f);
            EXPECT_FLOAT_EQ(height, 88.0f);
            EXPECT_EQ(element.queries, 1);
            const UISize desired = element.MeasureDesired();
            EXPECT_FLOAT_EQ(desired.width, width);
            EXPECT_FLOAT_EQ(desired.height, height);
            EXPECT_EQ(element.queries, 1); // Native retention still reuses the result.
            const UISize constrained = element.MeasureDesired(Budget(50.0f, 40.0f));
            EXPECT_FLOAT_EQ(constrained.width, 50.0f);
            EXPECT_FLOAT_EQ(constrained.height, 40.0f);
            EXPECT_EQ(element.queries, 2);
            element.Layout({0.0f, 0.0f, 500.0f, 500.0f});
            EXPECT_FLOAT_EQ(element.GetBounds().Width(), 99.0f);
            EXPECT_FLOAT_EQ(element.GetBounds().Height(), 88.0f);

            element.desired = {77.0f, 66.0f};
            element.MarkLayoutDirty();
            base.Measure(width, height);
            EXPECT_FLOAT_EQ(width, 77.0f);
            EXPECT_FLOAT_EQ(height, 66.0f);
            const int queries = element.queries;
            element.MeasureDesired();
            EXPECT_EQ(element.queries, queries);
        }

        template <typename Panel>
        void ExpectForcedPanelRetainsLogicalAllocation()
        {
            Panel panel;
            auto  probe = std::make_unique<WrappingProbe>();
            auto *child = probe.get();
            panel.AddChild(std::move(probe));
            const UIRect bounds{1000000.0f, 0.0f, 1000012.5f, 100.0f};
            const UISize allocation{12.49995f, 100.0f};
            panel.ArrangeIfNeeded(bounds, allocation);
            const int arrangements = child->arrangeCount;
            panel.Layout(); // A forced reflow preserves the prior logical slot.
            EXPECT_GT(child->arrangeCount, arrangements);
            EXPECT_FLOAT_EQ(panel.GetArrangedSize().width, allocation.width);
            EXPECT_FLOAT_EQ(panel.GetArrangedSize().height, allocation.height);
            EXPECT_FLOAT_EQ(child->GetArrangedSize().width, allocation.width);
        }

        UILayoutParams Rules(UISizeRule width, UISizeRule height = {})
        {
            UILayoutParams layout;
            layout.width  = width;
            layout.height = height;
            return layout;
        }
    } // namespace

    TEST(UIMeasureTest, NormalizedRejectsInvalidLimitsAndPreservesHardMaxima)
    {
        const float   infinity   = std::numeric_limits<float>::infinity();
        const float   nan        = std::numeric_limits<float>::quiet_NaN();
        UIConstraints normalized = UIConstraints{{nan, -2.0f}, {-1.0f, nan}}.Normalized();
        EXPECT_FLOAT_EQ(normalized.minSize.width, 0.0f);
        EXPECT_FLOAT_EQ(normalized.minSize.height, 0.0f);
        EXPECT_FLOAT_EQ(normalized.maxSize.width, 0.0f);
        EXPECT_FLOAT_EQ(normalized.maxSize.height, 0.0f);

        normalized = UIConstraints{{infinity, -infinity}, {infinity, -infinity}}.Normalized();
        EXPECT_FLOAT_EQ(normalized.minSize.width, 0.0f);
        EXPECT_FLOAT_EQ(normalized.minSize.height, 0.0f);
        EXPECT_EQ(normalized.maxSize.width, infinity);
        EXPECT_FLOAT_EQ(normalized.maxSize.height, 0.0f);

        normalized = UIConstraints{{100.0f, 80.0f}, {20.0f, 30.0f}}.Normalized();
        EXPECT_FLOAT_EQ(normalized.minSize.width, 20.0f);
        EXPECT_FLOAT_EQ(normalized.minSize.height, 30.0f);
        EXPECT_FLOAT_EQ(normalized.maxSize.width, 20.0f);
        EXPECT_FLOAT_EQ(normalized.maxSize.height, 30.0f);
    }

    TEST(UIMeasureTest, ConstrainSanitizesDesiredSizesAndAlwaysReturnsFiniteValues)
    {
        const float         infinity = std::numeric_limits<float>::infinity();
        const float         nan      = std::numeric_limits<float>::quiet_NaN();
        const UIConstraints limits{{5.0f, 7.0f}, {20.0f, infinity}};
        UISize              size = limits.Constrain({nan, -10.0f});
        EXPECT_FLOAT_EQ(size.width, 5.0f);
        EXPECT_FLOAT_EQ(size.height, 7.0f);
        size = limits.Constrain({infinity, infinity});
        EXPECT_FLOAT_EQ(size.width, 20.0f);
        EXPECT_FLOAT_EQ(size.height, 7.0f);
        EXPECT_TRUE(std::isfinite(size.width));
        EXPECT_TRUE(std::isfinite(size.height));
        size = limits.Constrain({100.0f, -infinity});
        EXPECT_FLOAT_EQ(size.width, 20.0f);
        EXPECT_FLOAT_EQ(size.height, 7.0f);
    }

    TEST(UIMeasureTest, DeflateSanitizesInsetsAndExcessivePaddingStaysInsideBounds)
    {
        const float   infinity = std::numeric_limits<float>::infinity();
        const float   nan      = std::numeric_limits<float>::quiet_NaN();
        UIConstraints deflated = UIConstraints{{10.0f, 20.0f}, {100.0f, infinity}}.Deflate(-3.0f, nan, infinity, 5.0f);
        EXPECT_FLOAT_EQ(deflated.minSize.width, 10.0f);
        EXPECT_FLOAT_EQ(deflated.minSize.height, 15.0f);
        EXPECT_FLOAT_EQ(deflated.maxSize.width, 100.0f);
        EXPECT_EQ(deflated.maxSize.height, infinity);
        deflated = UIConstraints{{5.0f, 6.0f}, {10.0f, 20.0f}}.Deflate(20.0f, 30.0f, 40.0f, 50.0f);
        EXPECT_FLOAT_EQ(deflated.minSize.width, 0.0f);
        EXPECT_FLOAT_EQ(deflated.minSize.height, 0.0f);
        EXPECT_FLOAT_EQ(deflated.maxSize.width, 0.0f);
        EXPECT_FLOAT_EQ(deflated.maxSize.height, 0.0f);

        UILayoutParams layout;
        layout.paddingLeft = layout.paddingTop = layout.paddingRight = layout.paddingBottom = 100.0f;
        const UIRect content                                                                = layout.ContentRect({10.0f, 20.0f, 30.0f, 40.0f});
        EXPECT_FLOAT_EQ(content.left, 30.0f);
        EXPECT_FLOAT_EQ(content.top, 40.0f);
        EXPECT_FLOAT_EQ(content.right, content.left);
        EXPECT_FLOAT_EQ(content.bottom, content.top);
        layout.paddingLeft     = -3.0f;
        layout.paddingTop      = nan;
        layout.paddingRight    = infinity;
        layout.paddingBottom   = 5.0f;
        const UIRect sanitized = layout.ContentRect({10.0f, 20.0f, 30.0f, 40.0f});
        EXPECT_FLOAT_EQ(sanitized.left, 10.0f);
        EXPECT_FLOAT_EQ(sanitized.top, 20.0f);
        EXPECT_FLOAT_EQ(sanitized.right, 30.0f);
        EXPECT_FLOAT_EQ(sanitized.bottom, 35.0f);
    }

    TEST(UIMeasureTest, MeasureDesiredCentrallyClampsNewAndLegacyCallbacks)
    {
        ConstraintProbe element;
        element.desired      = {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()};
        const UISize desired = element.MeasureDesired({{5.0f, 7.0f}, {20.0f, 30.0f}});
        EXPECT_FLOAT_EQ(desired.width, 5.0f);
        EXPECT_FLOAT_EQ(desired.height, 30.0f);
        EXPECT_FLOAT_EQ(element.received.minSize.width, 5.0f);
        EXPECT_FLOAT_EQ(element.received.maxSize.height, 30.0f);
        const UISize unbounded = element.MeasureDesired({{5.0f, 7.0f}, {20.0f, std::numeric_limits<float>::infinity()}});
        EXPECT_FLOAT_EQ(unbounded.height, 7.0f);
        const UISize normalized = element.MeasureDesired({{100.0f, -2.0f}, {20.0f, std::numeric_limits<float>::quiet_NaN()}});
        EXPECT_FLOAT_EQ(element.received.minSize.width, 20.0f);
        EXPECT_FLOAT_EQ(element.received.maxSize.height, 0.0f);
        EXPECT_FLOAT_EQ(normalized.width, 20.0f);
        EXPECT_FLOAT_EQ(normalized.height, 0.0f);
        LegacyProbe  legacy(300.0f, 400.0f);
        const UISize bounded = legacy.MeasureDesired(Budget(100.0f, 80.0f));
        EXPECT_FLOAT_EQ(bounded.width, 100.0f);
        EXPECT_FLOAT_EQ(bounded.height, 80.0f);
    }

    TEST(UIMeasureTest, OwnFixedRulesCanOverflowButAnAllocatedSlotRemainsAuthoritative)
    {
        ConstraintProbe element;
        element.SetLayout(Rules({UISizeMode::FIXED, 250.0f}, {UISizeMode::FIXED, 60.0f}));
        const UISize requested = element.MeasureForLayout(Budget(100.0f, 50.0f));
        EXPECT_FLOAT_EQ(requested.width, 250.0f);
        EXPECT_FLOAT_EQ(requested.height, 60.0f);
        EXPECT_FLOAT_EQ(element.received.minSize.width, 250.0f);
        EXPECT_FLOAT_EQ(element.received.maxSize.width, 250.0f);
        element.Layout({10.0f, 20.0f, 110.0f, 70.0f});
        EXPECT_FLOAT_EQ(element.GetBounds().right, 260.0f);
        EXPECT_FLOAT_EQ(element.GetBounds().bottom, 80.0f);

        const UISize allocated = element.MeasureDesired({{20.0f, 10.0f}, {20.0f, 10.0f}});
        EXPECT_FLOAT_EQ(allocated.width, 20.0f);
        EXPECT_FLOAT_EQ(allocated.height, 10.0f);
        EXPECT_FLOAT_EQ(element.received.maxSize.width, 20.0f);
        element.Arrange({3.0f, 4.0f, 23.0f, 14.0f});
        EXPECT_FLOAT_EQ(element.GetBounds().Width(), 20.0f);
        EXPECT_FLOAT_EQ(element.GetBounds().Height(), 10.0f);
    }

    TEST(UIMeasureTest, FillPercentAndAnchorsSupplyKnownBudgetsBeforeLayout)
    {
        ConstraintProbe element;
        UILayoutParams  layout = Rules({UISizeMode::FILL, 0.0f});
        layout.offsetLeft      = 3.0f;
        layout.offsetRight     = 7.0f;
        element.SetLayout(layout);
        EXPECT_FLOAT_EQ(element.MeasureForLayout(Budget(200.0f, 80.0f)).width, 190.0f);
        EXPECT_FLOAT_EQ(element.received.minSize.width, 190.0f);
        EXPECT_FLOAT_EQ(element.received.maxSize.width, 190.0f);
        element.Layout({10.0f, 20.0f, 210.0f, 100.0f});
        EXPECT_FLOAT_EQ(element.GetBounds().left, 13.0f);
        EXPECT_FLOAT_EQ(element.GetBounds().Width(), element.received.maxSize.width);

        layout.width = {UISizeMode::PERCENT, 0.25f};
        element.SetLayout(layout);
        EXPECT_FLOAT_EQ(element.MeasureForLayout(Budget(200.0f, 80.0f)).width, 50.0f);
        EXPECT_FLOAT_EQ(element.received.minSize.width, 50.0f);
        EXPECT_FLOAT_EQ(element.received.maxSize.width, 50.0f);
        element.Layout({10.0f, 20.0f, 210.0f, 100.0f});
        EXPECT_FLOAT_EQ(element.GetBounds().Width(), 50.0f);

        layout.anchorMinX = layout.anchorMinY = 0.25f;
        layout.anchorMaxX = layout.anchorMaxY = 0.75f;
        layout.offsetTop                      = 2.0f;
        layout.offsetBottom                   = 4.0f;
        element.SetLayout(layout);
        const UISize anchored = element.MeasureForLayout(Budget(200.0f, 80.0f));
        EXPECT_FLOAT_EQ(anchored.width, 90.0f);
        EXPECT_FLOAT_EQ(anchored.height, 34.0f);
        EXPECT_FLOAT_EQ(element.received.minSize.width, 90.0f);
        EXPECT_FLOAT_EQ(element.received.maxSize.width, 90.0f);
        EXPECT_FLOAT_EQ(element.received.minSize.height, 34.0f);
        EXPECT_FLOAT_EQ(element.received.maxSize.height, 34.0f);
        element.Layout({10.0f, 20.0f, 210.0f, 100.0f});
        EXPECT_FLOAT_EQ(element.GetBounds().left, 63.0f);
        EXPECT_FLOAT_EQ(element.GetBounds().top, 42.0f);
        EXPECT_FLOAT_EQ(element.GetBounds().Width(), element.received.maxSize.width);
        EXPECT_FLOAT_EQ(element.GetBounds().Height(), element.received.maxSize.height);
    }

    TEST(UIMeasureTest, UnboundedRuleAwareQueriesKeepIntrinsicFiniteSizes)
    {
        ConstraintProbe element;
        element.SetLayout(Rules({UISizeMode::FILL, 0.0f}, {UISizeMode::PERCENT, 0.5f}));
        const UISize desired = element.MeasureForLayout();
        EXPECT_FLOAT_EQ(desired.width, 30.0f);
        EXPECT_FLOAT_EQ(desired.height, 12.0f);
        EXPECT_EQ(element.received.maxSize.width, std::numeric_limits<float>::infinity());
        EXPECT_EQ(element.received.maxSize.height, std::numeric_limits<float>::infinity());
        EXPECT_FLOAT_EQ(element.received.minSize.width, 0.0f);
        EXPECT_FLOAT_EQ(element.received.minSize.height, 0.0f);
        EXPECT_TRUE(std::isfinite(desired.width));
        EXPECT_TRUE(std::isfinite(desired.height));
    }

    TEST(UIMeasureTest, LegacyFallbackHonorsConstraintsWithoutChangingElementState)
    {
        LegacyProbe element;
        element.Arrange({5.0f, 7.0f, 35.0f, 19.0f});
        element.ClearPaintDirty();
        const UISize desired = element.MeasureDesired({{20.0f, 8.0f}, {25.0f, 10.0f}});
        EXPECT_FLOAT_EQ(desired.width, 25.0f);
        EXPECT_FLOAT_EQ(desired.height, 10.0f);
        EXPECT_FLOAT_EQ(element.GetBounds().left, 5.0f);
        EXPECT_FLOAT_EQ(element.GetBounds().bottom, 19.0f);
        EXPECT_FALSE(element.IsLayoutDirty());
        EXPECT_FALSE(element.IsPaintDirty());
        EXPECT_TRUE(element.GetChildren().empty());
    }

    TEST(UIMeasureTest, NativeSubclassesUseOnMeasureForLegacyAndConstrainedQueries)
    {
        ExpectNativeMeasurementEntriesAgree<Text>();
        ExpectNativeMeasurementEntriesAgree<HBox>();
        ExpectNativeMeasurementEntriesAgree<VBox>();
        ExpectNativeMeasurementEntriesAgree<StackPanel>();
        ExpectNativeMeasurementEntriesAgree<DockPanel>();
        ExpectNativeMeasurementEntriesAgree<GridPanel>();
        ExpectNativeMeasurementEntriesAgree<Overlay>();
        ExpectNativeMeasurementEntriesAgree<ScrollView>();
        ExpectNativeMeasurementEntriesAgree<ListView>();
    }

    TEST(UIMeasureTest, ForcedPanelLayoutPreservesLogicalAllocation)
    {
        ExpectForcedPanelRetainsLogicalAllocation<DockPanel>();
        ExpectForcedPanelRetainsLogicalAllocation<GridPanel>();
        ExpectForcedPanelRetainsLogicalAllocation<StackPanel>();
    }

    TEST(UIMeasureTest, LegacyOnlySubclassKeepsConservativeExternalStateQueries)
    {
        LegacyProbe element(99.0f, 88.0f);
        float       width  = 0.0f;
        float       height = 0.0f;
        element.Measure(width, height);
        const UISize desired = element.MeasureDesired();
        EXPECT_FLOAT_EQ(desired.width, width);
        EXPECT_FLOAT_EQ(desired.height, height);
        EXPECT_TRUE(element.IsLayoutVolatile());
        const UISize bounded = element.MeasureDesired(Budget(50.0f, 40.0f));
        EXPECT_FLOAT_EQ(bounded.width, 50.0f);
        EXPECT_FLOAT_EQ(bounded.height, 40.0f);
    }

    TEST(UIMeasureTest, FractionalTextOriginsPreserveMeasuredLinesAndNarrowBudgetsWrap)
    {
        MeasurementTextures textures;
        FractionalFont      font;
        UITextSystem        textSystem(&font, &textures);
        const float         origins[]  = {0.0f, 0.1f, 127.9f, 500.1f, -500.1f, 10000.3f, -10000.3f};
        const UISize        paddings[] = {{0.0f, 0.0f}, {0.1f, 0.3f}, {3.3f, 4.7f}, {0.1f, 0.2f}, {0.0f, 0.1f}};
        for (float origin : origins) {
            for (UISize padding : paddings) {
                for (float contentWidth : {12.2f, 12.19998f, 12.1999f, 12.1f, 0.0f}) {
                    SCOPED_TRACE(::testing::Message()
                                 << "origin=" << origin << " padding=" << padding.width << "," << padding.height << " width=" << contentWidth);
                    Text text;
                    text.SetTextSystem(&textSystem);
                    text.SetContent("AB");
                    text.SetWrap(true);
                    UILayoutParams layout = Rules({UISizeMode::FIXED, contentWidth + padding.width + padding.height});
                    layout.offsetLeft     = origin;
                    layout.paddingLeft    = padding.width;
                    layout.paddingRight   = padding.height;
                    text.SetLayout(layout);
                    const UISize measured       = text.MeasureForLayout(Budget(20000.0f));
                    const float  expectedHeight = contentWidth == 12.2f ? 10.0f : 20.0f;
                    EXPECT_FLOAT_EQ(measured.height, expectedHeight);
                    text.Layout({0.0f, 0.0f, 20000.0f, 100.0f});
                    EXPECT_FLOAT_EQ(text.GetBounds().Height(), expectedHeight);
                    UIPaintContext context;
                    context.Begin({-20000.0f, 0.0f, 40000.0f, 100.0f});
                    text.Paint(context);
                    const auto &vertices = context.GetDrawData().vertices;
                    ASSERT_EQ(vertices.size(), 8u);
                    EXPECT_FLOAT_EQ(vertices[0].y, 0.0f);
                    EXPECT_FLOAT_EQ(vertices[4].y, expectedHeight - 10.0f);
                }
            }
        }
    }

    TEST(UIMeasureTest, ExplicitAllocationPreservesFractionalTightSlotAndKeysIncrementalArrangement)
    {
        MeasurementTextures textures;
        FractionalFont      font;
        UITextSystem        textSystem(&font, &textures);
        AllocatedText       text;
        text.SetTextSystem(&textSystem);
        text.SetContent("AB");
        text.SetWrap(true);
        UILayoutParams layout;
        layout.paddingLeft  = 0.1f;
        layout.paddingRight = 0.2f;
        text.SetLayout(layout);
        const UIRect slot{1000000.0f, 0.0f, 1000012.5f, 100.0f};
        const UISize exact{12.5f, 100.0f};
        const UISize narrower{12.49995f, 100.0f};
        EXPECT_FLOAT_EQ(1000000.0f + exact.width, 1000000.0f + narrower.width);
        text.ArrangeIfNeeded(slot, exact);
        EXPECT_EQ(text.arrangements, 1);
        ExpectTextLines(text, 10.0f);
        text.ArrangeIfNeeded(slot, exact);
        EXPECT_EQ(text.arrangements, 1);
        text.ArrangeIfNeeded(slot, narrower);
        EXPECT_EQ(text.arrangements, 2);
        ExpectTextLines(text, 20.0f);

        text.ClearPaintDirty();
        const UISize queried = text.MeasureDesired({exact, exact});
        EXPECT_FLOAT_EQ(queried.width, exact.width);
        EXPECT_FLOAT_EQ(text.GetArrangedSize().width, narrower.width);
        EXPECT_FALSE(text.IsLayoutDirty());
        EXPECT_FALSE(text.IsPaintDirty());
        ExpectTextLines(text, 20.0f); // An unrelated query cannot change the allocation.

        text.ArrangeIfNeeded(slot); // Rect-only explicitly uses endpoint differences.
        EXPECT_FLOAT_EQ(text.GetArrangedSize().width, slot.Width());
        EXPECT_EQ(text.arrangements, 3);
        ExpectTextLines(text, 10.0f);
    }

    TEST(UIMeasureTest, RuleAwareIncrementalLayoutIncludesLogicalParentBudget)
    {
        MeasurementTextures textures;
        FractionalFont      font;
        UITextSystem        textSystem(&font, &textures);
        AllocatedText       text;
        text.SetTextSystem(&textSystem);
        text.SetContent("AB");
        text.SetWrap(true);
        UILayoutParams layout = Rules({UISizeMode::FILL, 0.0f});
        layout.paddingLeft    = 0.1f;
        layout.paddingRight   = 0.2f;
        text.SetLayout(layout);
        const UIRect parent{1000000.0f, 0.0f, 1000012.5f, 100.0f};
        text.LayoutIfNeeded(parent, {12.5f, 100.0f});
        EXPECT_FLOAT_EQ(text.GetArrangedSize().height, 10.0f);
        const int arrangements = text.arrangements;
        text.LayoutIfNeeded(parent, {12.5f, 100.0f});
        EXPECT_EQ(text.arrangements, arrangements);
        text.LayoutIfNeeded(parent, {12.49995f, 100.0f});
        EXPECT_GT(text.arrangements, arrangements);
        EXPECT_FLOAT_EQ(text.GetArrangedSize().height, 20.0f);
        ExpectTextLines(text, 20.0f);
    }

    TEST(UIMeasureTest, ContainersPreserveLogicalTextWidthBeforeAddingFractionalOrigin)
    {
        MeasurementTextures textures;
        FractionalFont      font;
        UITextSystem        textSystem(&font, &textures);
        const auto          makeText = [&]() {
            auto text = std::make_unique<Text>();
            text->SetTextSystem(&textSystem);
            text->SetContent("AB");
            text->SetWrap(true);
            UILayoutParams layout = Rules({UISizeMode::FILL, 0.0f});
            layout.paddingLeft    = 0.1f;
            layout.paddingRight   = 0.2f;
            text->SetLayout(layout);
            return text;
        };
        std::vector<UIElementPtr> containers;
        containers.emplace_back(std::make_unique<UIElement>());
        containers.emplace_back(std::make_unique<HBox>());
        containers.emplace_back(std::make_unique<VBox>());
        containers.emplace_back(std::make_unique<StackPanel>());
        containers.emplace_back(std::make_unique<DockPanel>());
        containers.emplace_back(std::make_unique<GridPanel>());
        containers.emplace_back(std::make_unique<Overlay>());
        auto scroll = std::make_unique<ScrollView>();
        scroll->SetScrollAxes(ScrollAxes::Vertical);
        containers.emplace_back(std::move(scroll));
        for (auto &container : containers) {
            SCOPED_TRACE(container->GetTypeName());
            Text *text = static_cast<Text *>(container->AddChild(makeText()));
            container->Arrange({500.1f, 0.0f, 512.6f, 100.0f}, {12.5f, 100.0f});
            EXPECT_FLOAT_EQ(text->GetArrangedSize().width, 12.5f);
            EXPECT_FLOAT_EQ(text->GetContentSize().width, text->GetLayout().ContentSize({12.5f, 100.0f}).width);
            ExpectTextLines(*text, 10.0f);
            container->Arrange({500.1f, 0.0f, 512.6f, 100.0f}, {12.49995f, 100.0f});
            ExpectTextLines(*text, 20.0f);
        }
        ListView list;
        list.SetItemCount(1);
        list.SetItemHeight(10.0f);
        Text *row = nullptr;
        list.SetItemFactory([&](size_t) {
            auto text = makeText();
            row       = text.get();
            return text;
        });
        list.Arrange({500.1f, 0.0f, 512.6f, 10.0f}, {12.5f, 10.0f});
        ASSERT_NE(row, nullptr);
        EXPECT_FLOAT_EQ(row->GetArrangedSize().width, 12.5f);
        ExpectTextLines(*row, 10.0f);
    }

    TEST(UIMeasureTest, NestedPaddingDeflatesLogicalAllocationBeforeAbsolutePositioning)
    {
        MeasurementTextures textures;
        FractionalFont      font;
        UITextSystem        textSystem(&font, &textures);
        VBox                outer;
        UILayoutParams      outerLayout;
        outerLayout.paddingLeft  = 0.1f;
        outerLayout.paddingRight = 0.2f;
        outer.SetLayout(outerLayout);
        auto stack = std::make_unique<StackPanel>();
        stack->SetLayout(Rules({UISizeMode::FILL, 0.0f}));
        stack->SetPadding(0.1f);
        auto  text  = std::make_unique<Text>();
        Text *label = text.get();
        text->SetTextSystem(&textSystem);
        text->SetContent("AB");
        text->SetWrap(true);
        UILayoutParams textLayout;
        textLayout.paddingRight = 0.1f;
        text->SetLayout(textLayout);
        StackSlot slot;
        slot.width = StackSize::Fill;
        stack->Add(std::move(text), slot);
        outer.AddChild(std::move(stack));
        for (float origin : {500.1f, -500.1f}) {
            for (float width : {12.8f, 12.79995f}) {
                SCOPED_TRACE(::testing::Message() << origin << "," << width);
                const UISize measured = outer.MeasureDesired({{width, 0.0f}, {width, 100.0f}});
                outer.Arrange({origin, 0.0f, origin + width, measured.height}, {width, measured.height});
                EXPECT_FLOAT_EQ(label->GetContentSize().width, label->GetLayout().ContentSize(label->GetArrangedSize()).width);
                ExpectTextLines(*label, width == 12.8f ? 10.0f : 20.0f);
                EXPECT_FLOAT_EQ(measured.height, (width == 12.8f ? 10.0f : 20.0f) + 0.2f);
            }
        }
    }

    TEST(UIMeasureTest, HBoxAllocatesFixedPercentAndFillBeforeWrappingHeight)
    {
        HBox box;
        box.SetSpacing(5.0f);
        auto *fixed = box.AddChild(std::make_unique<LegacyProbe>());
        fixed->SetLayout(Rules({UISizeMode::FIXED, 40.0f}));
        auto *percent = box.AddChild(std::make_unique<LegacyProbe>());
        percent->SetLayout(Rules({UISizeMode::PERCENT, 0.25f}));
        auto  first     = std::make_unique<WrappingProbe>();
        auto *firstFill = first.get();
        firstFill->SetLayout(Rules({UISizeMode::FILL, 0.0f}));
        box.AddChild(std::move(first));
        auto  second     = std::make_unique<WrappingProbe>();
        auto *secondFill = second.get();
        secondFill->SetLayout(Rules({UISizeMode::FILL, 0.0f}));
        box.AddChild(std::move(second));

        const UISize desired = box.MeasureDesired(Budget(200.0f));
        EXPECT_FLOAT_EQ(desired.width, 200.0f);
        EXPECT_FLOAT_EQ(desired.height, 60.0f); // 240 / 47.5 rounds to six lines.
        EXPECT_FLOAT_EQ(firstFill->lastConstraints.maxSize.width, 47.5f);
        EXPECT_EQ(firstFill->arrangeCount, 0);
        box.Arrange({0.0f, 0.0f, desired.width, desired.height});
        EXPECT_FLOAT_EQ(percent->GetBounds().Width(), 50.0f);
        EXPECT_FLOAT_EQ(firstFill->GetBounds().left, 100.0f);
        EXPECT_FLOAT_EQ(firstFill->GetBounds().Width(), 47.5f);
        EXPECT_FLOAT_EQ(secondFill->GetBounds().left, 152.5f);
        EXPECT_FLOAT_EQ(secondFill->GetBounds().right, 200.0f);
    }

    TEST(UIMeasureTest, VBoxSharesRemainingHeightAndPreservesCrossAxisRules)
    {
        VBox box;
        box.SetSpacing(2.0f);
        auto *fixed = box.AddChild(std::make_unique<LegacyProbe>());
        fixed->SetLayout(Rules({UISizeMode::PERCENT, 0.5f}, {UISizeMode::FIXED, 20.0f}));
        auto *firstFill = box.AddChild(std::make_unique<LegacyProbe>());
        firstFill->SetLayout(Rules({UISizeMode::FILL, 0.0f}, {UISizeMode::FILL, 0.0f}));
        auto *secondFill = box.AddChild(std::make_unique<LegacyProbe>());
        secondFill->SetLayout(firstFill->GetLayout());
        box.Arrange({0.0f, 0.0f, 100.0f, 100.0f});
        EXPECT_FLOAT_EQ(fixed->GetBounds().Width(), 50.0f);
        EXPECT_FLOAT_EQ(firstFill->GetBounds().Height(), 38.0f);
        EXPECT_FLOAT_EQ(secondFill->GetBounds().top, 62.0f);
        EXPECT_FLOAT_EQ(secondFill->GetBounds().bottom, 100.0f);
    }

    TEST(UIMeasureTest, NestedAutoContainersRemeasureAtWidthBeforeFirstPaintAndAfterResize)
    {
        VBox root;
        auto stack = std::make_unique<StackPanel>(StackOrientation::Vertical);
        stack->SetLayout(Rules({UISizeMode::FILL, 0.0f}));
        auto      wrap  = std::make_unique<WrappingProbe>();
        auto     *probe = wrap.get();
        StackSlot slot;
        slot.width = StackSize::Fill;
        stack->Add(std::move(wrap), slot);
        auto *nested = root.AddChild(std::move(stack));

        UISize desired = root.MeasureDesired(Budget(80.0f));
        EXPECT_FLOAT_EQ(desired.height, 30.0f);
        EXPECT_EQ(probe->arrangeCount, 0);
        EXPECT_FLOAT_EQ(probe->GetBounds().Width(), 0.0f);
        root.Arrange({10.0f, 20.0f, 90.0f, 20.0f + desired.height});
        EXPECT_FLOAT_EQ(probe->GetBounds().Width(), 80.0f);
        EXPECT_FLOAT_EQ(nested->GetBounds().Height(), 30.0f);

        desired = root.MeasureDesired(Budget(40.0f));
        EXPECT_FLOAT_EQ(desired.height, 60.0f);
        EXPECT_FLOAT_EQ(probe->GetBounds().Width(), 80.0f); // Query cannot arrange.
        root.Arrange({10.0f, 20.0f, 50.0f, 20.0f + desired.height});
        EXPECT_FLOAT_EQ(probe->GetBounds().Width(), 40.0f);
        EXPECT_FLOAT_EQ(probe->GetBounds().Height(), 60.0f);
    }

    TEST(UIMeasureTest, StackSlotsOwnSizingAndIncludeOuterMarginsInFillBudget)
    {
        StackPanel stack;
        stack.SetGap(4.0f);
        StackSlot fixedSlot;
        fixedSlot.width  = StackSize::Fixed;
        fixedSlot.fixedW = 30.0f;
        fixedSlot.margin = 2.0f;
        auto fixed       = std::make_unique<LegacyProbe>();
        fixed->SetLayout(Rules({UISizeMode::FIXED, 900.0f}));
        auto     *fixedChild = stack.Add(std::move(fixed), fixedSlot);
        StackSlot fillSlot;
        fillSlot.width  = StackSize::Fill;
        fillSlot.margin = 3.0f;
        auto  wrap      = std::make_unique<WrappingProbe>();
        auto *fillChild = wrap.get();
        stack.Add(std::move(wrap), fillSlot);
        const UISize desired = stack.MeasureDesired(Budget(100.0f));
        EXPECT_FLOAT_EQ(desired.width, 100.0f);
        EXPECT_FLOAT_EQ(desired.height, 56.0f); // 56 available width, five lines + margins.
        stack.Arrange({0.0f, 0.0f, 100.0f, desired.height});
        EXPECT_FLOAT_EQ(fixedChild->GetBounds().Width(), 30.0f);
        EXPECT_FLOAT_EQ(fillChild->GetBounds().left, 41.0f);
        EXPECT_FLOAT_EQ(fillChild->GetBounds().Width(), 56.0f);
        EXPECT_FLOAT_EQ(fillChild->GetBounds().right + 3.0f, 100.0f);
    }

    TEST(UIMeasureTest, GridSpansAndMarginsDetermineIntrinsicAndWrappedSizes)
    {
        GridPanel grid;
        grid.SetGrid(2, 2);
        auto  wrap  = std::make_unique<WrappingProbe>(100.0f, 200.0f);
        auto *probe = wrap.get();
        probe->SetLayout(Rules({UISizeMode::FIXED, 900.0f}));
        grid.Add(std::move(wrap), {0, 0, 2, 2, 2.0f});
        UISize desired = grid.MeasureDesired();
        EXPECT_FLOAT_EQ(desired.width, 104.0f);
        EXPECT_FLOAT_EQ(desired.height, 24.0f);
        desired = grid.MeasureDesired(Budget(54.0f));
        EXPECT_FLOAT_EQ(desired.width, 54.0f);
        EXPECT_FLOAT_EQ(desired.height, 44.0f);
        EXPECT_FLOAT_EQ(probe->lastConstraints.maxSize.width, 50.0f);
        grid.Arrange({0.0f, 0.0f, desired.width, desired.height});
        EXPECT_FLOAT_EQ(probe->GetBounds().Width(), 50.0f);
        EXPECT_FLOAT_EQ(probe->GetBounds().Height(), 40.0f);
    }

    TEST(UIMeasureTest, DockConsumesEdgesInOrderAndDoesNotCountInnerMarginsTwice)
    {
        DockPanel dock;
        dock.Add(std::make_unique<LegacyProbe>(80.0f, 100.0f), {DockSide::Top, 20.0f, 2.0f});
        dock.Add(std::make_unique<LegacyProbe>(100.0f, 30.0f), {DockSide::Left, 30.0f, 2.0f});
        auto  wrap  = std::make_unique<WrappingProbe>(100.0f, 200.0f);
        auto *probe = wrap.get();
        dock.Add(std::move(wrap), {DockSide::Fill, 0.0f, 2.0f});
        const UISize natural = dock.MeasureDesired();
        EXPECT_FLOAT_EQ(natural.width, 134.0f);
        EXPECT_FLOAT_EQ(natural.height, 54.0f);
        const UISize narrow = dock.MeasureDesired(Budget(84.0f));
        EXPECT_FLOAT_EQ(narrow.width, 84.0f);
        EXPECT_FLOAT_EQ(narrow.height, 64.0f);
        EXPECT_FLOAT_EQ(probe->lastConstraints.maxSize.width, 50.0f);
        dock.Arrange({0.0f, 0.0f, narrow.width, narrow.height});
        EXPECT_FLOAT_EQ(probe->GetBounds().left, 32.0f);
        EXPECT_FLOAT_EQ(probe->GetBounds().Width(), 50.0f);
        EXPECT_FLOAT_EQ(probe->GetBounds().Height(), 40.0f);
    }

    TEST(UIMeasureTest, OverlayHasIntrinsicSizeAndGenericPaddingIsAppliedOnce)
    {
        Overlay        overlay;
        UILayoutParams params;
        params.paddingLeft   = 3.0f;
        params.paddingRight  = 5.0f;
        params.paddingTop    = 2.0f;
        params.paddingBottom = 4.0f;
        overlay.SetLayout(params);
        auto  wrap  = std::make_unique<WrappingProbe>(100.0f, 200.0f);
        auto *probe = wrap.get();
        overlay.AddChild(std::move(wrap));
        const UISize desired = overlay.MeasureDesired(Budget(58.0f));
        EXPECT_FLOAT_EQ(desired.width, 58.0f);
        EXPECT_FLOAT_EQ(desired.height, 46.0f);
        overlay.Arrange({0.0f, 0.0f, desired.width, desired.height});
        EXPECT_FLOAT_EQ(probe->GetBounds().left, 3.0f);
        EXPECT_FLOAT_EQ(probe->GetBounds().Width(), 50.0f);
        float width  = 0.0f;
        float height = 0.0f;
        overlay.Measure(width, height);
        EXPECT_FLOAT_EQ(width, 108.0f);
        EXPECT_FLOAT_EQ(height, 26.0f);
    }

    TEST(UIMeasureTest, UnboundedFillAndPercentUseIntrinsicSizes)
    {
        HBox  box;
        auto *fill = box.AddChild(std::make_unique<LegacyProbe>());
        fill->SetLayout(Rules({UISizeMode::FILL, 0.0f}));
        auto *percent = box.AddChild(std::make_unique<LegacyProbe>(20.0f, 15.0f));
        percent->SetLayout(Rules({UISizeMode::PERCENT, 0.5f}));
        const UISize desired = box.MeasureDesired();
        EXPECT_FLOAT_EQ(desired.width, 50.0f);
        EXPECT_FLOAT_EQ(desired.height, 15.0f);
        EXPECT_TRUE(std::isfinite(desired.width));
    }
} // namespace sky::ui
