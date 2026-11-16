//
// Created on 2026/11/15.
//

#include <editor/widgets/TooltipHost.h>
#include <editor/widgets/UiSkin.h>
#include <gtest/gtest.h>
#include <ui/UIContext.h>
#include <ui/UIPaintContext.h>
#include <ui/text/UIBuiltinFontProvider.h>
#include <ui/text/UITextSystem.h>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace sky::editor {
    namespace {

        class TooltipProbe : public ui::UIElement {
        public:
            explicit TooltipProbe(const ui::UIRect &bounds) : regions{bounds}
            {
                SetBounds(bounds);
            }

            bool QueryTooltip(float x, float y, ui::UITooltipTarget &target) const override
            {
                for (size_t i = 0; i < regions.size(); ++i) {
                    if (regions[i].Contains(x, y)) {
                        target = {i + 1, regions[i], lifetime};
                        return true;
                    }
                }
                return false;
            }

            bool BuildTooltip(uint64_t id, std::string &text) const override
            {
                ++builds;
                lastId = id;
                text   = content;
                if (onBuild) {
                    onBuild();
                }
                return true;
            }

            bool WantsPointerCapture() const override
            {
                return capture;
            }

            std::vector<ui::UIRect> regions;
            std::shared_ptr<void>   lifetime = std::make_shared<char>();
            std::string             content  = "Asset source path";
            std::function<void()>   onBuild;
            mutable uint32_t        builds  = 0;
            mutable uint64_t        lastId  = 0;
            bool                    capture = false;
        };

        TooltipProbe &AddProbe(ui::UIElement &root, const ui::UIRect &bounds)
        {
            auto  probe  = std::make_unique<TooltipProbe>(bounds);
            auto &result = *probe;
            root.AddChild(std::move(probe));
            return result;
        }

        void Move(TooltipHost &host, float x, float y)
        {
            host.Pointer({.action = ui::UIPointerAction::MOVE, .x = x, .y = y});
        }

        std::optional<ui::UIRect> TextClip(const ui::UIPaintContext &paint)
        {
            for (const auto &command : paint.GetDrawData().commands) {
                if (command.textureId != ui::UI_INVALID_TEXTURE) {
                    return command.clip;
                }
            }
            return std::nullopt;
        }

    } // namespace

    TEST(TooltipHostTest, BuildsLazilyOnceAndRestartsWhenTheRegionChanges)
    {
        ui::UIElement root;
        root.SetBounds({0, 0, 600, 400});
        auto &probe   = AddProbe(root, {20, 20, 220, 50});
        probe.regions = {{20, 20, 120, 50}, {120, 20, 220, 50}};
        TooltipHost host;
        Move(host, 30, 30);
        host.Tick(&root, 0.2f);
        host.Tick(&root, 0.29f);
        EXPECT_EQ(probe.builds, 0u);
        EXPECT_FALSE(host.IsVisible());
        host.Tick(&root, 0.02f);
        ASSERT_TRUE(host.IsVisible());
        EXPECT_EQ(probe.lastId, 1u);
        host.Tick(&root, 10.0f);
        EXPECT_EQ(probe.builds, 1u);

        Move(host, 150, 30);
        host.Tick(&root, 0.2f);
        EXPECT_FALSE(host.IsVisible());
        EXPECT_EQ(probe.builds, 1u);
        host.Tick(&root, 0.31f);
        EXPECT_TRUE(host.IsVisible());
        EXPECT_EQ(probe.lastId, 2u);
        EXPECT_EQ(probe.builds, 2u);
    }

    TEST(TooltipHostTest, RequiresAStationaryPointerBeforeShowingButKeepsVisibleRegionStable)
    {
        ui::UIElement root;
        root.SetBounds({0, 0, 600, 400});
        auto       &probe = AddProbe(root, {20, 20, 220, 50});
        TooltipHost host;
        Move(host, 30, 30);
        host.Tick(&root, 0.4f);
        Move(host, 40, 30);
        host.Tick(&root, 0.3f);
        EXPECT_FALSE(host.IsVisible());
        EXPECT_EQ(probe.builds, 0u);
        Move(host, 40, 30);
        host.Tick(&root, 0.21f);
        ASSERT_TRUE(host.IsVisible());
        EXPECT_EQ(probe.builds, 1u);
        Move(host, 50, 30);
        host.Tick(&root, 0.0f);
        EXPECT_TRUE(host.IsVisible());
        EXPECT_EQ(probe.builds, 1u);
    }

    TEST(TooltipHostTest, RebindingAndReattachingInvalidatePendingAndVisibleTargets)
    {
        ui::UIElement root;
        root.SetBounds({0, 0, 600, 400});
        auto       &probe = AddProbe(root, {20, 20, 220, 50});
        TooltipHost host;
        Move(host, 30, 30);
        host.Tick(&root, 0.4f);
        const auto previousBinding = probe.lifetime;
        probe.lifetime             = std::make_shared<char>();
        ASSERT_NE(previousBinding, probe.lifetime);
        host.Tick(&root, 0.2f);
        EXPECT_EQ(probe.builds, 0u);
        host.Tick(&root, 0.31f);
        ASSERT_TRUE(host.IsVisible());

        auto detached = root.RemoveChild(&probe);
        EXPECT_FALSE(host.IsVisible());
        root.AddChild(std::move(detached));
        host.Tick(&root, 0.3f);
        EXPECT_FALSE(host.IsVisible());
        EXPECT_EQ(probe.builds, 1u);
        host.Tick(&root, 0.21f);
        ASSERT_TRUE(host.IsVisible());
        probe.lifetime.reset();
        EXPECT_FALSE(host.IsVisible());
        host.Tick(&root, 1.0f);
        EXPECT_EQ(probe.builds, 2u);
    }

    TEST(TooltipHostTest, RejectsContentWhenTheBindingExpiresDuringBuild)
    {
        ui::UIElement root;
        root.SetBounds({0, 0, 600, 400});
        auto &probe   = AddProbe(root, {20, 20, 220, 50});
        probe.onBuild = [&probe]() { probe.lifetime.reset(); };
        TooltipHost host;
        Move(host, 30, 30);
        host.Tick(&root, 0.51f);
        EXPECT_EQ(probe.builds, 1u);
        EXPECT_FALSE(host.IsVisible());
    }

    TEST(TooltipHostTest, PopupCaptureQueriesOutsideItsBoundsAndBlocksBehindIt)
    {
        ui::UIElement root;
        root.SetBounds({0, 0, 600, 400});
        auto &behind  = AddProbe(root, {20, 20, 300, 300});
        auto &popup   = AddProbe(root, {20, 20, 220, 50});
        popup.capture = true;
        popup.regions = {{20, 60, 220, 90}};
        auto layout   = std::make_unique<ui::UIElement>();
        layout->SetBounds(root.GetBounds());
        root.AddChild(std::move(layout));
        TooltipHost host;
        Move(host, 30, 70);
        host.Tick({&root, popup.capture ? &popup : nullptr, root.GetLayoutRevision(), root.GetPaintRevision(), 0}, 0.51f);
        ASSERT_TRUE(host.IsVisible());
        EXPECT_EQ(popup.builds, 1u);
        EXPECT_EQ(behind.builds, 0u);

        Move(host, 30, 120);
        host.Tick({&root, &popup, root.GetLayoutRevision(), root.GetPaintRevision(), 0}, 1.0f);
        EXPECT_FALSE(host.IsVisible());
        EXPECT_EQ(behind.builds, 0u);
        popup.capture = false;
        host.Tick({&root, popup.capture ? &popup : nullptr, root.GetLayoutRevision(), root.GetPaintRevision(), 0}, 0.51f);
        EXPECT_FALSE(host.IsVisible());
        EXPECT_EQ(behind.builds, 0u);
        root.GetChildren().back()->SetVisible(false);
        host.Tick({&root, popup.capture ? &popup : nullptr, root.GetLayoutRevision(), root.GetPaintRevision(), 0}, 0.51f);
        EXPECT_TRUE(host.IsVisible());
        EXPECT_EQ(behind.builds, 1u);
    }

    TEST(TooltipHostTest, MapsSurfaceCoordinatesAndHonorsAncestorClipping)
    {
        ui::UIContext context;
        context.SetContentSize(600, 400);
        context.SetSurfaceTransform({.m00 = 2.0f, .m11 = 2.0f});
        auto  parent    = std::make_unique<ui::UIElement>();
        auto *parentPtr = parent.get();
        parent->SetBounds({0, 0, 60, 60});
        parent->SetClipsChildren(true);
        parent->SetTransform({.translationX = 20, .translationY = 10});
        auto &probe = AddProbe(*parent, {10, 10, 100, 40});
        context.AddChild(std::move(parent));
        TooltipHost host;
        Move(host, 80, 50);
        host.Tick(context.GetRoot(), 0.51f);
        ASSERT_TRUE(host.IsVisible());
        EXPECT_EQ(probe.builds, 1u);

        Move(host, 180, 50);
        host.Tick(context.GetRoot(), 1.0f);
        EXPECT_FALSE(host.IsVisible());
        EXPECT_EQ(probe.builds, 1u);
        parentPtr->SetClipsChildren(false);
        host.Tick(context.GetRoot(), 0.51f);
        ASSERT_TRUE(host.IsVisible());
        parentPtr->SetTransform({.scaleX = 0.0f});
        host.Tick(context.GetRoot(), 0.0f);
        EXPECT_FALSE(host.IsVisible());
    }

    TEST(TooltipHostTest, ClickWheelAndResetCancelAndButtonsKeepDragSuppressed)
    {
        ui::UIElement root;
        root.SetBounds({0, 0, 600, 400});
        auto       &probe = AddProbe(root, {20, 20, 220, 50});
        TooltipHost host;
        Move(host, 30, 30);
        host.Tick(&root, 0.51f);
        ASSERT_TRUE(host.IsVisible());
        host.Pointer({.action = ui::UIPointerAction::DOWN, .button = 0, .x = 30, .y = 30});
        host.Pointer({.action = ui::UIPointerAction::DOWN, .button = 1, .x = 30, .y = 30});
        host.Pointer({.action = ui::UIPointerAction::UP, .button = 0, .x = 30, .y = 30});
        Move(host, 30, 30);
        host.Tick(&root, 1.0f);
        EXPECT_FALSE(host.IsVisible());
        EXPECT_EQ(probe.builds, 1u);
        host.Pointer({.action = ui::UIPointerAction::UP, .button = 1, .x = 30, .y = 30});
        host.Tick(&root, 1.0f);
        EXPECT_FALSE(host.IsVisible());
        Move(host, 30, 30);
        host.Tick(&root, 0.51f);
        ASSERT_TRUE(host.IsVisible());
        host.Pointer({.action = ui::UIPointerAction::WHEEL, .x = 30, .y = 30, .wheelDelta = 1});
        host.Tick(&root, 1.0f);
        EXPECT_FALSE(host.IsVisible());
        Move(host, 30, 30);
        host.Tick(&root, 0.51f);
        ASSERT_TRUE(host.IsVisible());
        host.Reset();
        host.Tick(&root, 1.0f);
        EXPECT_FALSE(host.IsVisible());
    }

    TEST(TooltipHostTest, CancellationPreservesButtonsWhenDraggingBackIntoTheWindow)
    {
        ui::UIElement root;
        root.SetBounds({0, 0, 600, 400});
        auto       &probe = AddProbe(root, {20, 20, 220, 50});
        TooltipHost host;
        Move(host, 30, 30);
        host.Tick(&root, 0.51f);
        ASSERT_TRUE(host.IsVisible());
        host.Pointer({.action = ui::UIPointerAction::DOWN, .button = 0, .x = 30, .y = 30});
        host.Cancel();
        Move(host, 40, 30);
        host.Tick(&root, 1.0f);
        EXPECT_FALSE(host.IsVisible());
        EXPECT_EQ(probe.builds, 1u);
        host.Pointer({.action = ui::UIPointerAction::UP, .button = 0, .x = 40, .y = 30});
        Move(host, 40, 30);
        host.Tick(&root, 0.51f);
        EXPECT_TRUE(host.IsVisible());
        EXPECT_EQ(probe.builds, 2u);
    }

    TEST(TooltipHostTest, WrapsAndFlipsWithinTheSurfaceAfterNormalContent)
    {
        ui::UIContext context;
        context.SetContentSize(360, 240);
        auto &probe   = AddProbe(*context.GetRoot(), {310, 210, 355, 235});
        probe.content = "Builtin/Geometry/" + std::string(90, 'x');
        TooltipHost host;
        Move(host, 330, 220);
        host.Tick(context.GetRoot(), 0.51f);
        ASSERT_TRUE(host.IsVisible());
        ui::UIBuiltinFontProvider font;
        ui::UITextSystem          text(&font, nullptr);
        const auto                theme = MakeDarkTheme();
        UiSkin                    skin(theme, &text);
        ui::UIPaintContext        paint;
        context.Paint(paint);
        host.Paint(paint, context.GetRoot()->GetBounds(), skin);
        const auto clip = TextClip(paint);
        ASSERT_TRUE(clip.has_value());
        EXPECT_GE(clip->left, theme.metrics.panelGap + theme.metrics.controlPad);
        EXPECT_LE(clip->right, 360.0f - theme.metrics.panelGap - theme.metrics.controlPad);
        EXPECT_LT(clip->bottom, probe.GetBounds().top);
        EXPECT_GT(clip->Height(), font.GetFontMetrics(theme.fonts.label).lineHeight);
        EXPECT_FLOAT_EQ(paint.CurrentClip().right, 360.0f);
        EXPECT_FLOAT_EQ(paint.CurrentClip().bottom, 240.0f);
    }

} // namespace sky::editor

namespace sky::editor {
    TEST(TooltipHostTest, ForegroundWithoutTooltipOccludesLowerSibling)
    {
        ui::UIElement root;
        root.SetBounds({0, 0, 600, 400});
        auto &behind = AddProbe(root, {20, 20, 220, 50});
        auto  cover  = std::make_unique<ui::UIElement>();
        cover->SetBounds({20, 20, 220, 50});
        auto *foreground = cover.get();
        root.AddChild(std::move(cover));
        TooltipHost host;
        Move(host, 30, 30);
        host.Tick(&root, 1.f);
        EXPECT_FALSE(host.IsVisible());
        EXPECT_EQ(behind.builds, 0u);
        foreground->SetVisible(false);
        host.Tick(&root, .51f);
        EXPECT_TRUE(host.IsVisible());
    }

    TEST(TooltipHostTest, PaintValidationPreservesDelayAndRejectsHiddenOwners)
    {
        ui::UIElement root;
        root.SetBounds({0, 0, 600, 400});
        auto       &probe = AddProbe(root, {20, 20, 220, 50});
        TooltipHost host;
        Move(host, 30, 30);
        for (int i = 0; i < 6; ++i) {
            host.Tick(&root, .1f);
            host.ValidateRoot(&root);
        }
        ASSERT_TRUE(host.IsVisible());
        probe.SetVisible(false);
        host.ValidateRoot(&root);
        EXPECT_FALSE(host.IsVisible());
    }
} // namespace sky::editor

namespace sky::editor {
    TEST(TooltipHostTest, PaintValidationRejectsChangedGeometryWithoutSearchingAnotherTarget)
    {
        ui::UIElement root;
        root.SetBounds({0, 0, 600, 400});
        auto       &probe = AddProbe(root, {20, 20, 220, 50});
        TooltipHost host;
        Move(host, 30, 30);
        host.Tick(&root, .51f);
        ASSERT_TRUE(host.IsVisible());
        probe.SetTransform({.translationX = 300});
        host.ValidateRoot(&root);
        EXPECT_FALSE(host.IsVisible());
        EXPECT_EQ(probe.builds, 1u);
    }
} // namespace sky::editor

namespace sky::editor {
    TEST(TooltipHostTest, PaintRejectsNewOcclusionWithoutPointerMotion)
    {
        ui::UIElement root;
        root.SetBounds({0, 0, 600, 400});
        AddProbe(root, {20, 20, 220, 50});
        TooltipHost host;
        Move(host, 30, 30);
        host.Tick(&root, .51f);
        ASSERT_TRUE(host.IsVisible());
        auto cover = std::make_unique<ui::UIElement>();
        cover->SetBounds({20, 20, 220, 50});
        root.AddChild(std::move(cover));
        host.ValidateRoot(&root);
        EXPECT_FALSE(host.IsVisible());
    }
} // namespace sky::editor
