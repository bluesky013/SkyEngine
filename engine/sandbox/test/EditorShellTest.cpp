//
// Created on 2026/10/06.
//

#include <editor/shell/EditorShell.h>
#include <editor/core/layout/LayoutModel.h>
#include <editor/core/layout/PanelRegistry.h>
#include <ui/UIElement.h>
#include <gtest/gtest.h>
#include <memory>

using namespace sky::editor;

namespace {

    class TestPanel : public sky::ui::UIElement {
    public:
        const char *GetTypeName() const override { return "TestPanel"; }
    };

    sky::ui::UIPointerEvent Pointer(sky::ui::UIPointerAction action, float x, float y)
    {
        sky::ui::UIPointerEvent event;
        event.action = action;
        event.x = x;
        event.y = y;
        return event;
    }

    void RegisterTwoViews(EditorShell &shell)
    {
        shell.RegisterPanelView("a", []() { return std::make_unique<TestPanel>(); });
        shell.RegisterPanelView("b", []() { return std::make_unique<TestPanel>(); });
    }

} // namespace

TEST(EditorShellTest, ViewRegistrySurvivesRebuild)
{
    LayoutModel layout;
    layout.SetDefault({"a", "b"});
    PanelRegistry registry;
    registry.Register(PanelInfo{"a", "A", 0.0f, 0.0f, nullptr});
    registry.Register(PanelInfo{"b", "B", 0.0f, 0.0f, nullptr});

    EditorShell shell;
    shell.SetLayout(&layout);
    shell.SetPanelRegistry(&registry);
    int factoryCalls = 0;
    shell.RegisterPanelView("a", [&factoryCalls]() {
        ++factoryCalls;
        return std::make_unique<TestPanel>();
    });
    shell.RegisterPanelView("b", []() { return std::make_unique<TestPanel>(); });

    shell.Rebuild();
    sky::ui::UIElement *first = shell.GetPanelView("a");
    ASSERT_NE(first, nullptr);
    EXPECT_TRUE(shell.IsPanelAttached("a"));
    EXPECT_EQ(factoryCalls, 1);
    EXPECT_EQ(shell.GetPanelCount(), 1u); // one visible tab, counted once

    shell.Rebuild();
    sky::ui::UIElement *second = shell.GetPanelView("a");
    EXPECT_EQ(first, second);
    EXPECT_EQ(factoryCalls, 1);
}

TEST(EditorShellTest, SetPanelVisibleToggles)
{
    LayoutModel layout;
    layout.SetDefault({"viewport"});
    PanelRegistry registry;
    registry.Register(PanelInfo{"viewport", "Viewport", 0.0f, 0.0f, nullptr});
    registry.Register(PanelInfo{"outliner", "Outliner", 0.0f, 0.0f, nullptr});

    EditorShell shell;
    shell.SetLayout(&layout);
    shell.SetPanelRegistry(&registry);
    shell.RegisterPanelView("viewport", []() { return std::make_unique<TestPanel>(); });
    shell.RegisterPanelView("outliner", []() { return std::make_unique<TestPanel>(); });
    shell.Rebuild();

    EXPECT_TRUE(shell.HasPanel("viewport"));
    EXPECT_FALSE(shell.HasPanel("outliner"));

    shell.SetPanelVisible("outliner", true);
    shell.Layout(1280.0f, 720.0f);
    EXPECT_TRUE(shell.HasPanel("viewport"));
    EXPECT_TRUE(shell.HasPanel("outliner"));

    shell.SetPanelVisible("outliner", false);
    shell.Layout(1280.0f, 720.0f);
    EXPECT_FALSE(shell.HasPanel("outliner"));
}

TEST(EditorShellTest, LayoutDirtyTracksCommittedEdits)
{
    LayoutModel layout;
    layout.SetDefault({"viewport"});
    PanelRegistry registry;
    registry.Register(PanelInfo{"viewport", "Viewport", 0.0f, 0.0f, nullptr});
    registry.Register(PanelInfo{"outliner", "Outliner", 0.0f, 0.0f, nullptr});

    EditorShell shell;
    shell.SetLayout(&layout);
    shell.SetPanelRegistry(&registry);
    shell.RegisterPanelView("viewport", []() { return std::make_unique<TestPanel>(); });
    shell.RegisterPanelView("outliner", []() { return std::make_unique<TestPanel>(); });
    shell.Rebuild();

    EXPECT_FALSE(shell.ConsumeLayoutDirty());
    shell.SetPanelVisible("outliner", true);
    EXPECT_TRUE(shell.ConsumeLayoutDirty());
    EXPECT_FALSE(shell.ConsumeLayoutDirty());
}

TEST(EditorShellTest, FloatAndDockPanelSurface)
{
    LayoutModel layout;
    layout.SetDefault({"a", "b"});
    PanelRegistry registry;
    registry.Register(PanelInfo{"a", "A", 0.0f, 0.0f, nullptr});
    registry.Register(PanelInfo{"b", "B", 0.0f, 0.0f, nullptr});

    EditorShell shell;
    shell.SetLayout(&layout);
    shell.SetPanelRegistry(&registry);
    RegisterTwoViews(shell);
    shell.Rebuild();

    const uint32_t surfaceId = shell.FloatPanelToSurface("b", 7u, FloatingPanel{});
    EXPECT_EQ(surfaceId, 7u);
    EXPECT_TRUE(shell.IsPanelFloating("b"));
    EXPECT_EQ(shell.GetPanelSurface("b"), 7u);

    std::vector<std::string> panels;
    layout.CollectPanels(panels);
    ASSERT_EQ(panels.size(), 1u); // "b" left the dock tree

    shell.DockFloatingPanel("b", "a", DockPosition::CENTER);
    EXPECT_FALSE(shell.IsPanelFloating("b"));
    panels.clear();
    layout.CollectPanels(panels);
    EXPECT_EQ(panels.size(), 2u);
}

TEST(EditorShellTest, SplitterIsHittableAtSeam)
{
    LayoutModel layout;
    layout.SetDefault({"a"});
    ASSERT_TRUE(layout.SplitPanel("a", SplitOrientation::HORIZONTAL, "b"));

    PanelRegistry registry;
    registry.Register(PanelInfo{"a", "A", 0.0f, 0.0f, nullptr});
    registry.Register(PanelInfo{"b", "B", 0.0f, 0.0f, nullptr});

    EditorShell shell;
    shell.SetLayout(&layout);
    shell.SetPanelRegistry(&registry);
    RegisterTwoViews(shell);
    shell.Rebuild();
    shell.Layout(800.0f, 600.0f);

    sky::ui::UIElement *hit = shell.HitTest(400.0f, 300.0f);
    ASSERT_NE(hit, nullptr);
    EXPECT_STREQ(hit->GetTypeName(), "SplitterHandle");
}

TEST(EditorShellTest, DragSplitterChangesRatio)
{
    LayoutModel layout;
    layout.SetDefault({"a"});
    ASSERT_TRUE(layout.SplitPanel("a", SplitOrientation::HORIZONTAL, "b"));

    PanelRegistry registry;
    registry.Register(PanelInfo{"a", "A", 0.0f, 0.0f, nullptr});
    registry.Register(PanelInfo{"b", "B", 0.0f, 0.0f, nullptr});

    EditorShell shell;
    shell.SetLayout(&layout);
    shell.SetPanelRegistry(&registry);
    RegisterTwoViews(shell);
    shell.Rebuild();
    shell.Layout(800.0f, 600.0f);

    shell.DispatchPointer(Pointer(sky::ui::UIPointerAction::DOWN, 400.0f, 300.0f));
    shell.DispatchPointer(Pointer(sky::ui::UIPointerAction::MOVE, 600.0f, 300.0f));
    shell.DispatchPointer(Pointer(sky::ui::UIPointerAction::UP, 600.0f, 300.0f));

    auto *root = layout.GetRoot();
    ASSERT_TRUE(IsSplit(root));
    auto *split = static_cast<SplitNode *>(root);
    ASSERT_EQ(split->ratios.size(), 2u);
    EXPECT_NEAR(split->ratios[0], 0.75f, 0.03f);
}

TEST(EditorShellTest, TabCloseAffordance)
{
    LayoutModel layout;
    layout.SetDefault({"a", "b"});

    PanelRegistry registry;
    registry.Register(PanelInfo{"a", "A", 0.0f, 0.0f, nullptr});
    registry.Register(PanelInfo{"b", "B", 0.0f, 0.0f, nullptr});

    EditorShell shell;
    shell.SetLayout(&layout);
    shell.SetPanelRegistry(&registry);
    RegisterTwoViews(shell);
    shell.Rebuild();
    shell.Layout(800.0f, 600.0f);

    // Header row (content top + header height); first tab cell spans x in [0,400);
    // its close box sits at x >= 382.
    shell.DispatchPointer(Pointer(sky::ui::UIPointerAction::DOWN, 390.0f, 37.0f));
    shell.DispatchPointer(Pointer(sky::ui::UIPointerAction::UP, 390.0f, 37.0f));
    shell.Layout(800.0f, 600.0f);

    std::vector<std::string> panels;
    layout.CollectPanels(panels);
    ASSERT_EQ(panels.size(), 1u);
    EXPECT_EQ(panels[0], "b");
}

TEST(EditorShellTest, DragTabDocksIntoOtherTab)
{
    LayoutModel layout;
    layout.SetDefault({"a", "b"});
    ASSERT_TRUE(layout.SplitPanel("b", SplitOrientation::HORIZONTAL, "c"));
    ASSERT_NE(layout.FindTab("b"), layout.FindTab("c"));

    PanelRegistry registry;
    registry.Register(PanelInfo{"a", "A", 0.0f, 0.0f, nullptr});
    registry.Register(PanelInfo{"b", "B", 0.0f, 0.0f, nullptr});
    registry.Register(PanelInfo{"c", "C", 0.0f, 0.0f, nullptr});

    EditorShell shell;
    shell.SetLayout(&layout);
    shell.SetPanelRegistry(&registry);
    shell.RegisterPanelView("a", []() { return std::make_unique<TestPanel>(); });
    shell.RegisterPanelView("b", []() { return std::make_unique<TestPanel>(); });
    shell.RegisterPanelView("c", []() { return std::make_unique<TestPanel>(); });
    shell.Rebuild();
    shell.Layout(800.0f, 600.0f);

    // Tab1 header is the left half; its second cell ("b") spans x in [200,400).
    shell.DispatchPointer(Pointer(sky::ui::UIPointerAction::DOWN, 300.0f, 37.0f));
    shell.DispatchPointer(Pointer(sky::ui::UIPointerAction::MOVE, 600.0f, 300.0f));
    shell.DispatchPointer(Pointer(sky::ui::UIPointerAction::UP, 600.0f, 300.0f));
    shell.Layout(800.0f, 600.0f);

    // "b" tabified into the tab that holds "c".
    EXPECT_EQ(layout.FindTab("b"), layout.FindTab("c"));
    EXPECT_NE(layout.FindTab("b"), layout.FindTab("a"));
}
