#include "ReflectionTestEnvironment.h"
//
// Created on 2026/10/06.
//

#include <algorithm>
#include <core/type/TypeInfo.h>
#include <core/type/TypeInfoObj.h>
#include <cstdlib>
#include <editor/core/asset/EditorAssetCatalog.h>
#include <editor/core/document/WorldDocument.h>
#include <editor/core/extension/EditorActionRegistry.h>
#include <editor/core/extension/EditorScopeStack.h>
#include <editor/core/filebrowser/FileBrowserModel.h>
#include <editor/core/input/KeyModifiers.h>
#include <editor/core/layout/DefaultPanels.h>
#include <editor/core/layout/LayoutModel.h>
#include <editor/core/layout/PanelRegistry.h>
#include <editor/core/preferences/PreferencePageRegistry.h>
#include <editor/shell/AssetViewerWidget.h>
#include <editor/shell/EditorShell.h>
#include <editor/shell/FileBrowserDialog.h>
#include <editor/shell/ReflectionDemoPanel.h>
#include <editor/shell/WorldConfigPanel.h>
#include <editor/core/EditorCore.h>
#include <editor/shell/panels/AssetBrowserPanel.h>
#include <editor/ui/AssetViewProvider.h>
#include <editor/ui/UiTheme.h>
#include <editor/widgets/ActionBarBuilder.h>
#include <editor/widgets/MenuBar.h>
#include <editor/widgets/ReflectedFormView.h>
#include <editor/widgets/ToolBar.h>
#include <filesystem>
#include <framework/asset/AssetBuilderManager.h>
#include <framework/asset/AssetDataBase.h>
#include <framework/asset/CookConfig.h>
#include <framework/serialization/SerializationContext.h>
#include <framework/world/WorldSubSystemRegistry.h>
#include <fstream>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <ui/UIElement.h>
#include <ui/UIPaintContext.h>
#include <ui/text/UIBuiltinFontProvider.h>
#include <ui/text/UITextSystem.h>
#include <ui/widgets/StackPanel.h>
#include <vector>

#include "TestTypes.h"

using namespace sky::editor;

namespace {

    class TestPanel : public sky::ui::UIElement {
    public:
        const char *GetTypeName() const override
        {
            return "TestPanel";
        }
    };

    class ArrangedPanel : public sky::ui::UIElement {
    public:
        ArrangedPanel()
        {
            auto               stack = std::make_unique<sky::ui::StackPanel>(sky::ui::StackOrientation::Vertical);
            sky::ui::StackSlot slot;
            slot.width  = sky::ui::StackSize::Fill;
            slot.height = sky::ui::StackSize::Fixed;
            slot.fixedH = 24.0f;
            leaf        = stack->Add(std::make_unique<sky::ui::UIElement>(), slot);
            content     = AddChild(std::move(stack));
        }

        sky::ui::UIElement *leaf         = nullptr;
        int                 arrangements = 0;

    protected:
        void ArrangeChildren(const sky::ui::UIRect &rect) override
        {
            ++arrangements;
            content->Arrange(rect);
        }

    private:
        sky::ui::UIElement *content = nullptr;
    };

    sky::ui::UIPointerEvent Pointer(sky::ui::UIPointerAction action, float x, float y)
    {
        sky::ui::UIPointerEvent event;
        event.action = action;
        event.x      = x;
        event.y      = y;
        return event;
    }

    sky::ui::UIKeyEvent Key(uint32_t code)
    {
        sky::ui::UIKeyEvent event;
        event.keyCode = code;
        event.action  = sky::ui::UIKeyAction::DOWN;
        return event;
    }

    std::unique_ptr<FileBrowserDialog> MakeBrowser()
    {
        auto dialog = std::make_unique<FileBrowserDialog>(nullptr);
        dialog->SetBounds(sky::ui::UIRect{0.0f, 0.0f, 1000.0f, 700.0f});
        return dialog;
    }

    float CenterX(const sky::ui::UIRect &rect)
    {
        return (rect.left + rect.right) * 0.5f;
    }

    float CenterY(const sky::ui::UIRect &rect)
    {
        return (rect.top + rect.bottom) * 0.5f;
    }

    struct BrowserDirFixture {
        std::filesystem::path root;

        BrowserDirFixture()
        {
            static int counter = 0;
            root               = std::filesystem::temp_directory_path() / ("sky_shell_fb_" + std::to_string(++counter));
            std::error_code ec;
            std::filesystem::remove_all(root, ec);
            std::filesystem::create_directories(root / "sub");
            std::ofstream(root / "a.skyproj") << "x";
            std::ofstream(root / "b.txt") << "x";
        }

        ~BrowserDirFixture()
        {
            std::error_code ec;
            std::filesystem::remove_all(root, ec);
        }
    };

    FileBrowserRequest MakeOpenProjectRequest(const std::filesystem::path &dir)
    {
        FileBrowserRequest request;
        request.mode      = FileBrowserMode::OPEN_PROJECT;
        request.directory = dir.string();
        request.filters   = {{"SkyEngine Project", {"skyproj"}}};
        return request;
    }

    void RegisterTwoViews(EditorShell &shell)
    {
        shell.RegisterPanelView("a", []() { return std::make_unique<TestPanel>(); });
        shell.RegisterPanelView("b", []() { return std::make_unique<TestPanel>(); });
    }

    ReflectedFormView *FindForm(sky::ui::UIElement &element)
    {
        if (auto *form = dynamic_cast<ReflectedFormView *>(&element)) {
            return form;
        }
        for (const auto &child : element.GetChildren()) {
            if (auto *form = FindForm(*child)) {
                return form;
            }
        }
        return nullptr;
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

TEST(EditorShellTest, NestedPanelIsArrangedBeforeFirstPaint)
{
    LayoutModel layout;
    layout.SetDefault({"a"});
    PanelRegistry registry;
    registry.Register(PanelInfo{"a", "A", 0.0f, 0.0f, nullptr});

    EditorShell shell;
    shell.SetLayout(&layout);
    shell.SetPanelRegistry(&registry);
    ArrangedPanel *panel = nullptr;
    shell.RegisterPanelView("a", [&panel]() {
        auto view = std::make_unique<ArrangedPanel>();
        panel     = view.get();
        return view;
    });
    shell.Rebuild();
    shell.Layout(800.0f, 600.0f);

    ASSERT_NE(panel, nullptr);
    EXPECT_GT(panel->arrangements, 0);
    EXPECT_FLOAT_EQ(panel->leaf->GetBounds().Width(), panel->GetBounds().Width());
    EXPECT_FLOAT_EQ(panel->leaf->GetBounds().Height(), 24.0f);
    EXPECT_FLOAT_EQ(panel->leaf->GetBounds().top, panel->GetBounds().top);

    const int               arrangements = panel->arrangements;
    sky::ui::UIPaintContext paint;
    shell.Paint(paint);
    EXPECT_EQ(panel->arrangements, arrangements);
    EXPECT_FALSE(panel->leaf->IsLayoutDirty());
}

TEST(EditorShellTest, AssetViewerChromeIsArrangedBeforePaint)
{
    AssetViewerWidget viewer(nullptr);
    viewer.Open(sky::Uuid::Create());
    viewer.Arrange({0.0f, 0.0f, 1000.0f, 700.0f});

    sky::ui::UIElement *dock = nullptr;
    for (const auto &child : viewer.GetChildren()) {
        if (std::string(child->GetTypeName()) == "DockPanel") {
            dock = child.get();
        }
    }
    ASSERT_NE(dock, nullptr);
    ASSERT_EQ(dock->GetChildren().size(), 2u);
    const auto &toolbar = dock->GetChildren()[0];
    const auto &body    = dock->GetChildren()[1];
    EXPECT_FLOAT_EQ(toolbar->GetBounds().Height(), GetDefaultUiTheme().metrics.toolbarHeight);
    EXPECT_GT(body->GetBounds().Height(), 0.0f);
    EXPECT_FLOAT_EQ(body->GetBounds().top, toolbar->GetBounds().bottom);

    const sky::ui::UIRect   before = body->GetBounds();
    sky::ui::UIPaintContext paint;
    paint.Begin(viewer.GetBounds());
    viewer.Paint(paint);
    EXPECT_FLOAT_EQ(body->GetBounds().top, before.top);
    EXPECT_FLOAT_EQ(body->GetBounds().bottom, before.bottom);
    EXPECT_FALSE(body->IsLayoutDirty());
}

TEST(EditorShellTest, ConfigPanelArrangesFormWithoutPaintingHeader)
{
    WorldConfigPanel panel(nullptr, []() -> WorldDocument * { return nullptr; }, "Config");
    panel.SetTitleBarVisible(false);
    panel.Arrange({0.0f, 0.0f, 600.0f, 400.0f});

    const auto *form = FindForm(panel);
    ASSERT_NE(form, nullptr);
    EXPECT_FLOAT_EQ(form->GetBounds().top, 0.0f);
    EXPECT_FLOAT_EQ(form->GetBounds().left, GetDefaultUiTheme().metrics.listColumnWidth);
    EXPECT_FALSE(form->IsVisible());

    sky::ui::UIPaintContext paint;
    paint.Begin(panel.GetBounds());
    panel.Paint(paint);
    EXPECT_FLOAT_EQ(form->GetBounds().top, 0.0f);
    EXPECT_FALSE(form->IsLayoutDirty());
}

TEST(EditorShellTest, AssetsPanelRegistered)
{
    LayoutModel layout;
    layout.SetDefault({"outliner"});
    layout.Tabify("assets", "outliner");

    PanelRegistry registry;
    RegisterDefaultEditorPanels(registry);

    EditorShell shell;
    shell.SetLayout(&layout);
    shell.SetPanelRegistry(&registry);
    shell.SetAssetCatalog(EditorAssetCatalog::Get());
    shell.RegisterBuiltinPanelViews();
    shell.Rebuild();

    sky::ui::UIElement *view = shell.GetPanelView("assets");
    ASSERT_NE(view, nullptr);
    EXPECT_STREQ(view->GetTypeName(), "AssetBrowserPanel");
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

    // Header row is below the menu bar (24) + toolbar; first tab cell spans
    // x in [0,400) and its close box sits at x >= 382.
    const float headerRowY =
        GetDefaultUiTheme().metrics.menuBarHeight + GetDefaultUiTheme().metrics.toolbarHeight + GetDefaultUiTheme().metrics.tabHeaderHeight * 0.5f;
    shell.DispatchPointer(Pointer(sky::ui::UIPointerAction::DOWN, 390.0f, headerRowY));
    shell.DispatchPointer(Pointer(sky::ui::UIPointerAction::UP, 390.0f, headerRowY));
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
    const float headerRowY =
        GetDefaultUiTheme().metrics.menuBarHeight + GetDefaultUiTheme().metrics.toolbarHeight + GetDefaultUiTheme().metrics.tabHeaderHeight * 0.5f;
    shell.DispatchPointer(Pointer(sky::ui::UIPointerAction::DOWN, 300.0f, headerRowY));
    shell.DispatchPointer(Pointer(sky::ui::UIPointerAction::MOVE, 600.0f, 300.0f));
    shell.DispatchPointer(Pointer(sky::ui::UIPointerAction::UP, 600.0f, 300.0f));
    shell.Layout(800.0f, 600.0f);

    // "b" tabified into the tab that holds "c".
    EXPECT_EQ(layout.FindTab("b"), layout.FindTab("c"));
    EXPECT_NE(layout.FindTab("b"), layout.FindTab("a"));
}

TEST(EditorShellTest, OpenFileBrowserHostsModal)
{
    BrowserDirFixture fixture;
    LayoutModel       layout;
    layout.SetDefault({"viewport"});
    PanelRegistry registry;
    registry.Register(PanelInfo{"viewport", "Viewport", 0.0f, 0.0f, nullptr});

    EditorShell shell;
    shell.SetLayout(&layout);
    shell.SetPanelRegistry(&registry);
    shell.RegisterPanelView("viewport", []() { return std::make_unique<TestPanel>(); });
    shell.Rebuild();
    shell.Layout(1000.0f, 700.0f);

    EXPECT_FALSE(shell.IsFileBrowserOpen());

    FileBrowserResult  captured;
    bool               called = false;
    FileBrowserRequest request;
    request.mode        = FileBrowserMode::SELECT_DIRECTORY;
    request.directory   = fixture.root.string();
    request.defaultName = "MyProject";
    shell.OpenFileBrowser(request, [&](const FileBrowserResult &result) {
        called   = true;
        captured = result;
    });

    EXPECT_TRUE(shell.IsFileBrowserOpen());
    EXPECT_TRUE(shell.WantsInput());

    sky::ui::UIKeyEvent enter;
    enter.keyCode = 0x0D;
    enter.action  = sky::ui::UIKeyAction::DOWN;
    EXPECT_TRUE(shell.DispatchKey(enter));

    EXPECT_FALSE(shell.IsFileBrowserOpen());
    EXPECT_TRUE(called);
    EXPECT_TRUE(captured.accepted);
    EXPECT_EQ(std::filesystem::path(captured.path), fixture.root / "MyProject");
}

TEST(EditorShellTest, PreferencesHostsDialog)
{
    LayoutModel layout;
    layout.SetDefault({"viewport"});
    PanelRegistry registry;
    registry.Register(PanelInfo{"viewport", "Viewport", 0.0f, 0.0f, nullptr});

    EditorShell shell;
    shell.SetLayout(&layout);
    shell.SetPanelRegistry(&registry);
    shell.RegisterPanelView("viewport", []() { return std::make_unique<TestPanel>(); });
    shell.Rebuild();
    shell.Layout(1000.0f, 700.0f);

    PreferencePageRegistry *pages = PreferencePageRegistry::Get();
    pages->Clear();
    PreferencePage pageA;
    pageA.id    = "a";
    pageA.title = "A";
    pages->Add(std::move(pageA));
    PreferencePage pageB;
    pageB.id    = "b";
    pageB.title = "B";
    pages->Add(std::move(pageB));

    shell.SetPreferences(pages, []() {});

    EXPECT_FALSE(shell.IsPreferencesOpen());
    shell.OpenPreferences();
    EXPECT_TRUE(shell.IsPreferencesOpen());
    EXPECT_TRUE(shell.WantsInput());

    sky::ui::UIKeyEvent esc;
    esc.keyCode = 0x1B;
    esc.action  = sky::ui::UIKeyAction::DOWN;
    EXPECT_TRUE(shell.DispatchKey(esc));
    EXPECT_FALSE(shell.IsPreferencesOpen());
    pages->Clear();
}

TEST(EditorShellTest, WorldConfigPanelTogglesSubsystem)
{
    sky::WorldSubSystemRegistry::Get().Clear();
    sky::WorldSubSystemRegistry::Get().Register(sky::Name("Test"),
                                                sky::WorldSubSystemRegistration{
                                                    [](sky::World &, const sky::Any &) -> std::unique_ptr<sky::IWorldSubSystem> { return nullptr; },
                                                    nullptr,
                                                    {},
                                                    {},
                                                });

    const std::filesystem::path path = std::filesystem::temp_directory_path() / "world_config_panel_test.world";
    std::error_code             ec;
    std::filesystem::remove(path, ec);

    WorldDocument    document(path.string());
    WorldConfigPanel panel(nullptr, [&document]() { return &document; }, "Config");
    panel.SetBounds(sky::ui::UIRect{0.0f, 0.0f, 300.0f, 400.0f});

    // Merely displaying a subsystem does not enable it. The checkbox creates an undoable edit.
    sky::ui::UIPointerEvent down;
    down.action = sky::ui::UIPointerAction::DOWN;
    down.x      = 10.0f;
    down.y      = 41.0f; // first row's checkbox
    panel.OnPointerEvent(down);

    bool enabled = true;
    ASSERT_TRUE(document.IsSubSystemEnabled("Test", enabled));
    EXPECT_TRUE(enabled);
    EXPECT_TRUE(document.IsDirty());
    ASSERT_TRUE(EditorCore::GetCommandService().Undo());
    EXPECT_FALSE(document.IsSubSystemEnabled("Test", enabled));

    std::filesystem::remove(path, ec);
    sky::WorldSubSystemRegistry::Get().Clear();
}

TEST(EditorShellTest, ConfigPanelKeepsConfigValidAfterToggleRealloc)
{
    sky::WorldSubSystemRegistry::Get().Clear();
    auto reg = []() -> sky::WorldSubSystemRegistration {
        return sky::WorldSubSystemRegistration{
            [](sky::World &, const sky::Any &) -> std::unique_ptr<sky::IWorldSubSystem> { return nullptr; },
            sky::TypeInfoObj<test::TestConfig>::Get()->RtInfo(),
            [] { return sky::Any(std::in_place_type<test::TestConfig>); },
            {},
        };
    };
    sky::WorldSubSystemRegistry::Get().Register(sky::Name("CfgA"), reg());
    sky::WorldSubSystemRegistry::Get().Register(sky::Name("CfgB"), reg());

    const std::filesystem::path path = std::filesystem::temp_directory_path() / "world_config_panel_realloc_test.world";
    std::error_code             ec;
    std::filesystem::remove(path, ec);

    WorldDocument    document(path.string());
    WorldConfigPanel panel(nullptr, [&document]() { return &document; }, "Config");
    panel.SetBounds(sky::ui::UIRect{0.0f, 0.0f, 300.0f, 400.0f});

    ReflectedFormView *form = FindForm(panel);
    ASSERT_NE(form, nullptr);
    auto readGravity = [](ReflectedFormView *view) -> float {
        for (auto &section : view->Form().GetSections()) {
            for (auto &field : section.fields) {
                if (field.path == "gravity") {
                    const sky::Any value = field.descriptor.GetValue();
                    if (const float *g = value.GetAsConst<float>()) {
                        return *g;
                    }
                }
            }
        }
        return -1.0f;
    };
    EXPECT_FLOAT_EQ(readGravity(form), -9.81f); // initial bind

    // "CfgA" is row 0 (sorted); the ctor already selected/bound it. Toggling
    // "CfgB" (row 1) appends to WorldDesc::subSystems -> vector reallocation.
    sky::ui::UIPointerEvent toggle;
    toggle.action = sky::ui::UIPointerAction::DOWN;
    toggle.x      = 10.0f;
    toggle.y      = 65.0f; // row 1 checkbox
    panel.OnPointerEvent(toggle);

    // The panel must have rebound: the form still reads CfgA's default, not garbage.
    EXPECT_FLOAT_EQ(readGravity(form), -9.81f);

    std::filesystem::remove(path, ec);
    sky::WorldSubSystemRegistry::Get().Clear();
}

TEST(EditorShellTest, CtrlSInvokesSaveAction)
{
    EditorActionRegistry::Get()->Clear();
    int saves = 0;
    EditorActionRegistry::Get()->Add({.id = "file.save", .label = "Save", .invoke = [&saves]() { ++saves; }, .accelerator = "Ctrl+S"});

    EditorShell         shell;
    sky::ui::UIKeyEvent s;
    s.keyCode   = 'S';
    s.action    = sky::ui::UIKeyAction::DOWN;
    s.modifiers = 0x00C0; // Ctrl
    EXPECT_TRUE(shell.DispatchKey(s));
    EXPECT_EQ(saves, 1);

    s.modifiers = 0; // plain 'S' does not save
    shell.DispatchKey(s);
    EXPECT_EQ(saves, 1);
    EditorActionRegistry::Get()->Clear();
}

TEST(EditorShellTest, F5TogglesPlayPauseAndShiftStops)
{
    EditorShell shell;
    EditorActionRegistry::Get()->Clear();

    int   plays  = 0;
    int   pauses = 0;
    int   stops  = 0;
    auto *reg    = EditorActionRegistry::Get();
    reg->Add({.id      = "play.play",
              .label   = "Play",
              .enabled = [&shell]() { return shell.GetPlayState() != PlayState::Playing; },
              .invoke =
                  [&]() {
                      ++plays;
                      shell.SetPlayState(PlayState::Playing);
                  }});
    reg->Add({.id      = "play.pause",
              .label   = "Pause",
              .enabled = [&shell]() { return shell.GetPlayState() == PlayState::Playing; },
              .invoke =
                  [&]() {
                      ++pauses;
                      shell.SetPlayState(PlayState::Paused);
                  }});
    reg->Add({.id      = "play.stop",
              .label   = "Stop",
              .enabled = [&shell]() { return shell.GetPlayState() != PlayState::Editing; },
              .invoke =
                  [&]() {
                      ++stops;
                      shell.SetPlayState(PlayState::Editing);
                  }});

    sky::ui::UIKeyEvent f5;
    f5.keyCode = 0x74; // F5
    f5.action  = sky::ui::UIKeyAction::DOWN;

    EXPECT_TRUE(shell.DispatchKey(f5));
    EXPECT_EQ(plays, 1);
    EXPECT_EQ(shell.GetPlayState(), PlayState::Playing);

    EXPECT_TRUE(shell.DispatchKey(f5));
    EXPECT_EQ(pauses, 1);
    EXPECT_EQ(shell.GetPlayState(), PlayState::Paused);

    EXPECT_TRUE(shell.DispatchKey(f5)); // resume from paused
    EXPECT_EQ(plays, 2);
    EXPECT_EQ(shell.GetPlayState(), PlayState::Playing);

    f5.modifiers = 0x0003; // Shift
    EXPECT_TRUE(shell.DispatchKey(f5));
    EXPECT_EQ(stops, 1);
    EXPECT_EQ(shell.GetPlayState(), PlayState::Editing);
    EditorActionRegistry::Get()->Clear();
}

TEST(EditorShellTest, CtrlWInvokesCloseAction)
{
    EditorActionRegistry::Get()->Clear();
    int closes = 0;
    EditorActionRegistry::Get()->Add({.id = "file.close", .label = "Close", .invoke = [&closes]() { ++closes; }, .accelerator = "Ctrl+W"});

    EditorShell         shell;
    sky::ui::UIKeyEvent w;
    w.keyCode   = 'W';
    w.action    = sky::ui::UIKeyAction::DOWN;
    w.modifiers = 0x00C0; // Ctrl
    EXPECT_TRUE(shell.DispatchKey(w));
    EXPECT_EQ(closes, 1);

    w.modifiers = 0; // plain 'W' does not close
    shell.DispatchKey(w);
    EXPECT_EQ(closes, 1);
    EditorActionRegistry::Get()->Clear();
}

TEST(EditorShellTest, ScopedChromeFiltersByActiveScope)
{
    auto *reg = EditorActionRegistry::Get();
    reg->Clear();
    EditorScopeStack::Get()->Clear();

    reg->Add({.id = "global.save", .label = "Save", .group = EditorGroup::File, .invoke = []() {}});
    reg->Add(
        {.id = "assetViewer.save", .label = "Save", .group = EditorGroup::File, .invoke = []() {}, .scope = "assetViewer", .accelerator = "Ctrl+S"});

    const auto invoke = [](const std::string &) {};

    const auto globalItems = ActionBarBuilder::BuildToolBarItems(invoke);
    ASSERT_EQ(globalItems.size(), 1u);
    EXPECT_EQ(globalItems.front().id, "global.save");

    EditorScopeStack::Get()->Push("assetViewer");
    const auto scopedItems = ActionBarBuilder::BuildToolBarItems(invoke);
    ASSERT_EQ(scopedItems.size(), 1u);
    EXPECT_EQ(scopedItems.front().id, "assetViewer.save");
    EditorScopeStack::Get()->Pop("assetViewer");

    reg->Clear();
    EditorScopeStack::Get()->Clear();
}

TEST(EditorShellTest, ScopedAcceleratorResolvesInActiveScope)
{
    auto *reg = EditorActionRegistry::Get();
    reg->Clear();
    EditorScopeStack::Get()->Clear();

    int global = 0;
    int scoped = 0;
    reg->Add({.id = "file.save", .label = "Save", .invoke = [&global]() { ++global; }, .accelerator = "Ctrl+S"});
    reg->Add({.id = "assetViewer.save", .label = "Save", .invoke = [&scoped]() { ++scoped; }, .scope = "assetViewer", .accelerator = "Ctrl+S"});

    EditorShell         shell;
    sky::ui::UIKeyEvent s;
    s.keyCode   = 'S';
    s.action    = sky::ui::UIKeyAction::DOWN;
    s.modifiers = 0x00C0; // Ctrl

    EXPECT_TRUE(shell.DispatchKey(s));
    EXPECT_EQ(global, 1);
    EXPECT_EQ(scoped, 0);

    EditorScopeStack::Get()->Push("assetViewer");
    EXPECT_TRUE(shell.DispatchKey(s));
    EXPECT_EQ(global, 1);
    EXPECT_EQ(scoped, 1);
    EditorScopeStack::Get()->Pop("assetViewer");

    reg->Clear();
    EditorScopeStack::Get()->Clear();
}

TEST(EditorShellTest, UiMetricsScaleDoublesDimensionsAndFonts)
{
    UiMetrics scaled;
    scaled.Scale(2.0f);
    const UiMetrics base;
    EXPECT_FLOAT_EQ(scaled.rowHeight, base.rowHeight * 2.0f);
    EXPECT_FLOAT_EQ(scaled.frameHeight, base.frameHeight * 2.0f);
    EXPECT_FLOAT_EQ(scaled.listColumnWidth, base.listColumnWidth * 2.0f);
    EXPECT_FLOAT_EQ(scaled.hubRowHeight, base.hubRowHeight * 2.0f);

    const UiTheme one = MakeDarkTheme(1.0f);
    const UiTheme two = MakeDarkTheme(2.0f);
    EXPECT_FLOAT_EQ(two.metrics.rowHeight, one.metrics.rowHeight * 2.0f);
    EXPECT_FLOAT_EQ(two.scale, 2.0f);
    EXPECT_EQ(two.fonts.label, one.fonts.label * 2);
    EXPECT_EQ(two.fonts.banner, one.fonts.banner * 2);
}

TEST(EditorShellTest, DocumentInfoDrivesWindowTitle)
{
    EditorShell shell;
    shell.SetStatusInfo("Proj", "Vulkan", "Edit");

    std::string title;
    shell.SetTitleHandler([&title](const std::string &t) { title = t; });

    shell.SetDocumentInfo("world.world", false);
    EXPECT_EQ(title, "world.world - Proj - SkyEngine Editor");

    shell.SetDocumentInfo("world.world", true);
    EXPECT_EQ(title, "world.world* - Proj - SkyEngine Editor");
}

TEST(EditorShellTest, RevertIconClickResetsField)
{
    const sky::TypeNode *type = sky::GetTypeNode(sky::TypeInfo<test::TestObject>::RegisteredId());
    ASSERT_NE(type, nullptr);

    test::TestObject object; // default: value == 0
    object.value = 5.f;

    ReflectedFormView     view(nullptr, "");
    const sky::ui::UIRect bounds{0.0f, 0.0f, 300.0f, 240.0f};
    view.SetBounds(bounds);
    view.Bind(PropertyObject{&object, type});

    ASSERT_EQ(view.Form().GetSections().size(), 1u);
    const auto &fields   = view.Form().GetSections()[0].fields;
    int         rowIndex = -1;
    for (int i = 0; i < static_cast<int>(fields.size()); ++i) {
        if (fields[static_cast<std::size_t>(i)].path == "value") {
            rowIndex = i;
        }
    }
    ASSERT_GE(rowIndex, 0);
    ASSERT_TRUE(view.Form().IsModified(fields[static_cast<std::size_t>(rowIndex)]));

    // Mirror ReflectedFormView::BuildRows / LayoutField geometry to hit the
    // revert icon of the first row (outside the control rect).
    const UiMetrics &m           = GetDefaultUiTheme().metrics;
    const float      layoutRight = bounds.right - m.padX - m.scrollBarWidth;
    const float      rowsTop     = bounds.top + m.headerHeight + 4.0f + m.sectionHeight;
    const float      revertX     = layoutRight - 8.0f;
    const float      rowY        = rowsTop + (static_cast<float>(rowIndex) + 0.5f) * m.rowHeight;

    sky::ui::UIPointerEvent down;
    down.action = sky::ui::UIPointerAction::DOWN;
    down.x      = revertX;
    down.y      = rowY;
    view.OnPointerEvent(down);

    EXPECT_FLOAT_EQ(object.value, 0.f); // reset restored the type default
}

TEST(EditorShellTest, ToolBarHitAndEnabledState)
{
    ToolBar bar(nullptr);
    int     clicks = 0;
    bar.SetItems({
        {"open", "Open", sky::ui::UI_INVALID_TEXTURE, [&clicks]() { ++clicks; }, true, false},
        {"save", "Save", sky::ui::UI_INVALID_TEXTURE, [&clicks]() { ++clicks; }, false, false},
    });
    bar.SetBounds(sky::ui::UIRect{0.0f, 0.0f, 400.0f, 30.0f});

    const UiMetrics &m = GetDefaultUiTheme().metrics;
    // No text system => label width 0 => the item is padded to iconButtonWidth.
    const float w   = std::max(2.0f * m.controlPad, m.iconButtonWidth);
    const float cy  = 15.0f;
    const float i1l = m.padX + w + m.itemSpacing;

    sky::ui::UIPointerEvent down;
    down.action = sky::ui::UIPointerAction::DOWN;

    down.x = m.padX + w * 0.5f; // enabled "Open"
    down.y = cy;
    bar.OnPointerEvent(down);
    EXPECT_EQ(clicks, 1);

    down.x = i1l + w * 0.5f; // disabled "Save"
    bar.OnPointerEvent(down);
    EXPECT_EQ(clicks, 1);

    bar.SetItemEnabled("save", true);
    bar.OnPointerEvent(down);
    EXPECT_EQ(clicks, 2);
}

TEST(EditorShellTest, NewWorldDialogOpens)
{
    LayoutModel layout;
    layout.SetDefault({"viewport"});
    PanelRegistry registry;
    registry.Register(PanelInfo{"viewport", "Viewport", 0.0f, 0.0f, nullptr});

    EditorShell shell;
    shell.SetLayout(&layout);
    shell.SetPanelRegistry(&registry);
    shell.RegisterPanelView("viewport", []() { return std::make_unique<TestPanel>(); });
    shell.Rebuild();
    shell.Layout(1000.0f, 700.0f);

    EXPECT_FALSE(shell.WantsInput());
    shell.OpenNewWorldDialog("C:/tmp", "main", [](const std::string &) {});
    EXPECT_TRUE(shell.WantsInput()); // the new-world dialog is now the active modal
}

TEST(FileBrowserDialogTest, EscCancels)
{
    auto              dialog = MakeBrowser();
    FileBrowserResult captured;
    bool              called = false;
    dialog->SetOnResult([&](const FileBrowserResult &result) {
        called   = true;
        captured = result;
    });

    FileBrowserRequest request;
    request.mode      = FileBrowserMode::OPEN_FILE;
    request.directory = ".";
    dialog->Open(request);
    EXPECT_TRUE(dialog->IsOpen());

    dialog->OnKeyEvent(Key(0x1B));
    EXPECT_FALSE(dialog->IsOpen());
    EXPECT_TRUE(called);
    EXPECT_FALSE(captured.accepted);
}

TEST(FileBrowserDialogTest, EnterAcceptsSelectDirectory)
{
    BrowserDirFixture fixture;
    auto              dialog = MakeBrowser();
    FileBrowserResult captured;
    dialog->SetOnResult([&](const FileBrowserResult &result) { captured = result; });

    FileBrowserRequest request;
    request.mode        = FileBrowserMode::SELECT_DIRECTORY;
    request.directory   = fixture.root.string();
    request.defaultName = "MyProject";
    dialog->Open(request);

    dialog->OnKeyEvent(Key(0x0D));
    EXPECT_FALSE(dialog->IsOpen());
    EXPECT_TRUE(captured.accepted);
    EXPECT_TRUE(captured.directory);
    EXPECT_EQ(std::filesystem::path(captured.path), fixture.root / "MyProject");
}

TEST(FileBrowserDialogTest, NameFieldEditing)
{
    auto               dialog = MakeBrowser();
    FileBrowserRequest request;
    request.mode        = FileBrowserMode::SELECT_DIRECTORY;
    request.directory   = ".";
    request.defaultName = "MyProject";
    dialog->Open(request);

    // Double-clicking the field selects the whole name so typing replaces it.
    const sky::ui::UIRect nameRect = dialog->NameFieldRect();
    dialog->OnPointerEvent(Pointer(sky::ui::UIPointerAction::DOWN, CenterX(nameRect), CenterY(nameRect)));
    dialog->OnPointerEvent(Pointer(sky::ui::UIPointerAction::DOWN, CenterX(nameRect), CenterY(nameRect)));

    sky::ui::UITextInputEvent text;
    text.text = "Abc";
    dialog->OnTextInput(text);
    EXPECT_EQ(dialog->Model().GetName(), "Abc");

    dialog->OnKeyEvent(Key(0x08)); // backspace -> "Ab"
    EXPECT_EQ(dialog->Model().GetName(), "Ab");

    dialog->OnKeyEvent(Key(0x25)); // left -> caret before 'b'
    sky::ui::UITextInputEvent insert;
    insert.text = "X";
    dialog->OnTextInput(insert);
    EXPECT_EQ(dialog->Model().GetName(), "AXb");

    // Delete removes forward; Backspace trims from the end.
    dialog->OnKeyEvent(Key(0x2E)); // delete at caret 2 -> "AX"
    EXPECT_EQ(dialog->Model().GetName(), "AX");
    dialog->OnKeyEvent(Key(0x08)); // backspace -> "A"
    EXPECT_EQ(dialog->Model().GetName(), "A");
    dialog->OnKeyEvent(Key(0x08)); // backspace -> ""
    EXPECT_EQ(dialog->Model().GetName(), "");
}

TEST(FileBrowserDialogTest, CancelButtonRejects)
{
    BrowserDirFixture fixture;
    auto              dialog = MakeBrowser();
    FileBrowserResult captured;
    dialog->SetOnResult([&](const FileBrowserResult &result) { captured = result; });
    dialog->Open(MakeOpenProjectRequest(fixture.root));

    const sky::ui::UIRect cancel = dialog->ButtonRect(1);
    dialog->OnPointerEvent(Pointer(sky::ui::UIPointerAction::DOWN, CenterX(cancel), CenterY(cancel)));
    EXPECT_FALSE(dialog->IsOpen());
    EXPECT_FALSE(captured.accepted);
}

TEST(FileBrowserDialogTest, DoubleClickNavigatesIntoDirectoryAndUp)
{
    BrowserDirFixture fixture;
    auto              dialog = MakeBrowser();
    dialog->Open(MakeOpenProjectRequest(fixture.root));

    // Row 0 is "sub" (directories sort first).
    const sky::ui::UIRect row = dialog->RowRect(0);
    dialog->OnPointerEvent(Pointer(sky::ui::UIPointerAction::DOWN, CenterX(row), CenterY(row)));
    dialog->OnPointerEvent(Pointer(sky::ui::UIPointerAction::DOWN, CenterX(row), CenterY(row)));
    EXPECT_EQ(std::filesystem::path(dialog->Model().GetLocation()).filename().string(), "sub");

    const sky::ui::UIRect up = dialog->UpRect();
    dialog->OnPointerEvent(Pointer(sky::ui::UIPointerAction::DOWN, CenterX(up), CenterY(up)));
    EXPECT_EQ(std::filesystem::canonical(std::filesystem::path(dialog->Model().GetLocation())), std::filesystem::canonical(fixture.root));
}

TEST(FileBrowserDialogTest, OkAcceptsProjectFile)
{
    BrowserDirFixture fixture;
    auto              dialog = MakeBrowser();
    FileBrowserResult captured;
    dialog->SetOnResult([&](const FileBrowserResult &result) { captured = result; });
    dialog->Open(MakeOpenProjectRequest(fixture.root));

    // Rows: 0 = sub (dir), 1 = a.skyproj (b.txt is filtered out by the active *skyproj filter).
    const sky::ui::UIRect row = dialog->RowRect(1);
    dialog->OnPointerEvent(Pointer(sky::ui::UIPointerAction::DOWN, CenterX(row), CenterY(row)));
    const sky::ui::UIRect open = dialog->ButtonRect(0);
    dialog->OnPointerEvent(Pointer(sky::ui::UIPointerAction::DOWN, CenterX(open), CenterY(open)));

    EXPECT_FALSE(dialog->IsOpen());
    EXPECT_TRUE(captured.accepted);
    EXPECT_FALSE(captured.directory);
    EXPECT_EQ(std::filesystem::path(captured.path).filename().string(), "a.skyproj");
}

TEST(FileBrowserDialogTest, FooterClickDoesNotHitListRows)
{
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "sky_fb_many";
    std::error_code             ec;
    std::filesystem::remove_all(root, ec);
    std::filesystem::create_directories(root);
    for (int i = 0; i < 40; ++i) {
        std::ofstream(root / ("file" + std::to_string(i) + ".txt")) << "x";
    }

    auto               dialog = MakeBrowser();
    FileBrowserRequest request;
    request.mode      = FileBrowserMode::OPEN_FILE;
    request.directory = root.string();
    dialog->Open(request);

    const sky::ui::UIRect nameRect = dialog->NameFieldRect();
    const float           x        = CenterX(nameRect);
    const float           y        = CenterY(nameRect);
    dialog->OnPointerEvent(Pointer(sky::ui::UIPointerAction::DOWN, x, y));
    dialog->OnPointerEvent(Pointer(sky::ui::UIPointerAction::DOWN, x, y));

    EXPECT_TRUE(dialog->IsOpen());
    EXPECT_EQ(std::filesystem::path(dialog->Model().GetLocation()), root);
    EXPECT_TRUE(dialog->Model().GetName().empty());
    EXPECT_EQ(dialog->Model().GetSelected(), -1);

    std::filesystem::remove_all(root, ec);
}

TEST(FileBrowserDialogTest, FilterPopupSwitchesToAllFiles)
{
    BrowserDirFixture fixture;
    auto              dialog = MakeBrowser();
    dialog->Open(MakeOpenProjectRequest(fixture.root));

    // Active filter defaults to the first (*.skyproj): sub + a.skyproj.
    EXPECT_EQ(dialog->Model().GetActiveFilter(), 0);
    EXPECT_EQ(dialog->Model().GetEntries().size(), 2u);

    const sky::ui::UIRect filter = dialog->FilterRect();
    dialog->OnPointerEvent(Pointer(sky::ui::UIPointerAction::DOWN, CenterX(filter), CenterY(filter)));
    const sky::ui::UIRect all = dialog->FilterItemRect(0); // 0 == "All Files"
    dialog->OnPointerEvent(Pointer(sky::ui::UIPointerAction::DOWN, CenterX(all), CenterY(all)));

    EXPECT_EQ(dialog->Model().GetActiveFilter(), -1);
    EXPECT_EQ(dialog->Model().GetEntries().size(), 3u); // sub + a.skyproj + b.txt
}

namespace {

    struct ShellCookSettings {
        uint32_t maxSize = 0;
    };

    const auto *builderMetadata = sky::editor::test::AddReflectionEnvironment([] {
        sky::SerializationContext::Get()->Register<ShellCookSettings>("ShellTestCookSettings").Member<&ShellCookSettings::maxSize>("maxSize");
    });

    class ShellTextBuilder : public sky::AssetBuilder {
    public:
        ShellTextBuilder()
        {
        }

        const std::vector<std::string> &GetExtensions() const override
        {
            static const std::vector<std::string> exts{".txt"};
            return exts;
        }
        std::string_view QueryType(const std::string &ext) const override
        {
            return ext == ".txt" ? std::string_view("Text") : std::string_view{};
        }
        std::vector<std::pair<std::string, std::string>> DescribeSettings(const sky::ProductBundleKey &,
                                                                          const sky::BuildSettingsOverride &override) const override
        {
            const auto it = override.find("maxSize");
            return {{"maxSize", it != override.end() ? it->second : std::string("0")}};
        }
        const sky::TypeInfoRT *GetSettingsType() const override
        {
            return sky::TypeInfoObj<ShellCookSettings>::Get()->RtInfo();
        }
        sky::Any MakeSettings(const sky::ProductBundleKey &, const sky::BuildSettingsOverride &override) const override
        {
            ShellCookSettings settings;
            const auto        it = override.find("maxSize");
            if (it != override.end()) {
                settings.maxSize = static_cast<uint32_t>(std::strtoul(it->second.c_str(), nullptr, 10));
            }
            return sky::Any(settings);
        }
        sky::BuildSettingsOverride DiffSettings(const sky::ProductBundleKey &, const sky::Any &edited) const override
        {
            sky::BuildSettingsOverride out;
            const auto                *values = edited.GetAsConst<ShellCookSettings>();
            if (values != nullptr && values->maxSize != 0) {
                out["maxSize"] = std::to_string(values->maxSize);
            }
            return out;
        }
    };

    class ResponsiveAssetContent : public sky::ui::UIElement {
    public:
        std::vector<float> measuredWidths;
        float              heightScale  = 300000.0f;
        int                arrangements = 0;

    protected:
        sky::ui::UISize OnMeasure(const sky::ui::UIConstraints &constraints) override
        {
            const float width = constraints.maxSize.width;
            measuredWidths.push_back(width);
            return {width, heightScale / std::max(1.0f, width)};
        }
        void ArrangeChildren(const sky::ui::UIRect &) override
        {
            ++arrangements;
        }
    };

    class ResponsiveAssetViewProvider : public IAssetViewProvider {
    public:
        std::unique_ptr<sky::ui::UIElement> CreateContent(const sky::Uuid &, const std::string &, sky::ui::UITextSystem *) override
        {
            auto result = std::make_unique<ResponsiveAssetContent>();
            if (retained) {
                result->SetLayoutVolatile(false);
            }
            content = result.get();
            return result;
        }

        ResponsiveAssetContent *content  = nullptr;
        bool                    retained = false;
    };

    class CountingAssetViewer : public AssetViewerWidget {
    public:
        CountingAssetViewer() : AssetViewerWidget(nullptr)
        {
        }
        int arrangements = 0;

    protected:
        void ArrangeChildren(const sky::ui::UIRect &rect) override
        {
            ++arrangements;
            AssetViewerWidget::ArrangeChildren(rect);
        }
    };

    struct ScopedAssetViewerEnvironment {
        sky::AssetBuilderManager  *manager            = sky::AssetBuilderManager::Get();
        sky::AssetDataBase        *database           = sky::AssetDataBase::Get();
        EditorAssetCatalog        *catalog            = EditorAssetCatalog::Get();
        AssetViewProviderRegistry *providers          = AssetViewProviderRegistry::Get();
        sky::CookConfig            previousCookConfig = manager->GetCookConfig();
        sky::FileSystemPtr         previousWorkspace  = database->GetWorkSpaceFs();
        IAssetViewProvider        *previousProvider   = providers->Find("Text");
        sky::AssetBuilder         *temporaryBuilder   = nullptr;
        bool                       databaseChanged    = false;
        std::filesystem::path      root = std::filesystem::temp_directory_path() / ("sky_asset_viewer_width_" + sky::Uuid::Create().ToString());

        ~ScopedAssetViewerEnvironment()
        {
            if (previousProvider != nullptr) {
                providers->Register("Text", previousProvider);
            } else {
                providers->Unregister("Text");
            }
            manager->SetCookConfig(std::move(previousCookConfig));
            if (temporaryBuilder != nullptr) {
                manager->UnRegisterBuilder(temporaryBuilder);
            }
            if (databaseChanged) {
                database->Reset();
                database->SetWorkSpaceFs(previousWorkspace);
                EXPECT_FALSE(database->RebuildCacheFromScan().has_value());
                catalog->Refresh();
            }
            std::error_code ec;
            std::filesystem::remove_all(root, ec);
        }
    };

} // namespace

TEST(EditorShellTest, AssetViewerMeasuresResponsiveContentAtItsFinalWidth)
{
    ResponsiveAssetViewProvider provider;
    provider.retained = true;
    ScopedAssetViewerEnvironment environment;
    std::filesystem::create_directories(environment.root);
    std::ofstream(environment.root / "a.txt") << "responsive content";

    if (environment.manager->QueryBuilder(".txt") == nullptr) {
        environment.temporaryBuilder = new ShellTextBuilder();
        environment.manager->RegisterBuilder(environment.temporaryBuilder);
    }
    ASSERT_EQ(environment.manager->QueryBuilder(".txt")->QueryType(".txt"), "Text");
    sky::CookConfig cookConfig;
    ASSERT_TRUE(cookConfig.Parse(R"({"bundles":["common"],"presets":{"windows":["common"]}})"));
    cookConfig.SetActivePlatform("windows");
    environment.manager->SetCookConfig(std::move(cookConfig));

    environment.databaseChanged = true;
    environment.database->Reset();
    environment.database->SetWorkSpaceFs(new sky::NativeFileSystem(sky::FilePath(environment.root.string())));
    EXPECT_FALSE(environment.database->RebuildCacheFromScan().has_value());
    environment.catalog->Refresh();
    environment.providers->Register("Text", &provider);

    EditorAssetItem item;
    ASSERT_TRUE(environment.catalog->FindByPath("Project/a.txt", item));
    CountingAssetViewer viewer;
    viewer.SetCatalog(environment.catalog);
    viewer.Open(item.uuid);
    viewer.SynchronizeLayout();
    viewer.ArrangeIfNeeded({0.0f, 0.0f, 1000.0f, 700.0f});

    ResponsiveAssetContent *content = provider.content;
    ASSERT_NE(content, nullptr);
    ASSERT_NE(content->GetParent(), nullptr);
    ASSERT_GE(content->measuredWidths.size(), 2u);
    const sky::ui::UIRect wideBounds   = content->GetBounds();
    const sky::ui::UIRect wideViewport = content->GetParent()->GetBounds();
    EXPECT_GT(wideBounds.Width(), 0.0f);
    EXPECT_FLOAT_EQ(content->measuredWidths.back(), wideBounds.Width());
    EXPECT_NEAR(wideBounds.Height(), 300000.0f / wideBounds.Width(), 0.001f);
    // The clipping viewport includes the scrollbar column, while custom
    // content receives the narrower final column and therefore a taller size.
    EXPECT_GT(wideViewport.Width(), wideBounds.Width());
    EXPECT_GT(wideBounds.Height(), wideViewport.Height());
    EXPECT_GT(content->measuredWidths.front(), content->measuredWidths.back());

    const size_t idleMeasures     = content->measuredWidths.size();
    const int    idleArrangements = content->arrangements;
    viewer.arrangements           = 0;
    viewer.SynchronizeLayout();
    viewer.ArrangeIfNeeded(viewer.GetBounds());
    EXPECT_EQ(viewer.arrangements, 0);
    EXPECT_EQ(content->arrangements, idleArrangements);
    EXPECT_EQ(content->measuredWidths.size(), idleMeasures);

    sky::ui::UIPointerEvent wheel;
    wheel.action     = sky::ui::UIPointerAction::WHEEL;
    wheel.x          = wideBounds.left + 5.0f;
    wheel.y          = wideBounds.top + 5.0f;
    wheel.wheelDelta = -100.0f;
    viewer.OnPointerEvent(wheel);
    EXPECT_TRUE(viewer.IsLayoutDirty());
    viewer.SynchronizeLayout();
    viewer.ArrangeIfNeeded(viewer.GetBounds());
    EXPECT_EQ(viewer.arrangements, 1);
    EXPECT_LT(content->GetBounds().top, wideBounds.top);
    EXPECT_EQ(content->measuredWidths.size(), idleMeasures);

    const size_t wideMeasureCount = content->measuredWidths.size();
    viewer.ArrangeIfNeeded({0.0f, 0.0f, 600.0f, 460.0f});
    ASSERT_GT(content->measuredWidths.size(), wideMeasureCount);
    const sky::ui::UIRect narrowBounds = content->GetBounds();
    EXPECT_GT(narrowBounds.Width(), 0.0f);
    EXPECT_LT(narrowBounds.Width(), wideBounds.Width());
    EXPECT_GT(narrowBounds.Height(), wideBounds.Height());
    EXPECT_FLOAT_EQ(content->measuredWidths.back(), narrowBounds.Width());
    EXPECT_NEAR(narrowBounds.Height(), 300000.0f / narrowBounds.Width(), 0.001f);
    EXPECT_GT(content->GetParent()->GetBounds().Width(), narrowBounds.Width());

    const size_t            measureCountBeforePaint = content->measuredWidths.size();
    sky::ui::UIPaintContext paint;
    paint.Begin(viewer.GetBounds());
    viewer.Paint(paint);
    EXPECT_EQ(content->measuredWidths.size(), measureCountBeforePaint);
    EXPECT_FLOAT_EQ(content->GetBounds().left, narrowBounds.left);
    EXPECT_FLOAT_EQ(content->GetBounds().top, narrowBounds.top);
    EXPECT_FLOAT_EQ(content->GetBounds().right, narrowBounds.right);
    EXPECT_FLOAT_EQ(content->GetBounds().bottom, narrowBounds.bottom);

    // Providers with unnotified state keep the conservative layout policy.
    content->SetLayoutVolatile(true);
    viewer.SynchronizeLayout();
    viewer.ArrangeIfNeeded(viewer.GetBounds());
    content->heightScale = 400000.0f;
    viewer.SynchronizeLayout();
    viewer.ArrangeIfNeeded(viewer.GetBounds());
    EXPECT_NEAR(content->GetBounds().Height(), 400000.0f / content->GetBounds().Width(), 0.001f);

    auto settings = environment.catalog->GetCookSettings(item.uuid, "common");
    ASSERT_TRUE(settings.IsValid());
    ASSERT_NE(settings.object.GetAs<ShellCookSettings>(), nullptr);
    settings.object.GetAs<ShellCookSettings>()->maxSize = 512;
    ASSERT_TRUE(environment.catalog->ApplyCookSettings(item.uuid, "common", settings.object));
    viewer.Close();
    viewer.Open(item.uuid);
    viewer.SynchronizeLayout();
    viewer.ArrangeIfNeeded(viewer.GetBounds());
    ASSERT_NE(provider.content, nullptr);
    EXPECT_NEAR(provider.content->GetBounds().Height(), 300000.0f / provider.content->GetBounds().Width(), 0.001f);
    bool reloadedSettings = false;
    for (const auto &child : provider.content->GetParent()->GetChildren()) {
        if (auto *form = dynamic_cast<ReflectedFormView *>(child.get())) {
            for (const auto &section : form->Form().GetSections()) {
                for (const auto &field : section.fields) {
                    if (field.path == "maxSize") {
                        const auto  value = field.descriptor.GetValue();
                        const auto *size  = value.GetAsConst<uint32_t>();
                        reloadedSettings |= size != nullptr && *size == 512u;
                    }
                }
            }
        }
    }
    EXPECT_TRUE(reloadedSettings); // refreshed before any Paint of the reopened viewer
}

TEST(EditorShellTest, AssetViewerRefreshesThemeChromeAndReopensWithoutWindowResize)
{
    const UiTheme previous = GetDefaultUiTheme();
    struct RestoreTheme {
        UiTheme theme;
        ~RestoreTheme()
        {
            SetDefaultUiTheme(theme);
        }
    } restore{previous};
    CountingAssetViewer viewer;
    viewer.Open(sky::Uuid::Create());
    viewer.SynchronizeLayout();
    viewer.ArrangeIfNeeded({0.0f, 0.0f, 1000.0f, 700.0f});
    ToolBar *toolbar = nullptr;
    MenuBar *menu    = nullptr;
    for (const auto &child : viewer.GetChildren()) {
        if (auto *candidate = dynamic_cast<MenuBar *>(child.get())) {
            menu = candidate;
        }
        for (const auto &nested : child->GetChildren()) {
            if (auto *candidate = dynamic_cast<ToolBar *>(nested.get())) {
                toolbar = candidate;
            }
        }
    }
    ASSERT_NE(toolbar, nullptr);
    ASSERT_NE(menu, nullptr);
    EXPECT_FLOAT_EQ(toolbar->GetBounds().Height(), previous.metrics.toolbarHeight);
    viewer.arrangements = 0;
    viewer.SynchronizeLayout();
    viewer.ArrangeIfNeeded(viewer.GetBounds());
    EXPECT_EQ(viewer.arrangements, 0);

    UiTheme changed = previous;
    changed.metrics.toolbarHeight += 9.0f;
    changed.metrics.menuBarHeight += 3.0f;
    SetDefaultUiTheme(changed);
    viewer.SynchronizeLayout();
    viewer.ArrangeIfNeeded(viewer.GetBounds());
    EXPECT_EQ(viewer.arrangements, 1);
    EXPECT_FLOAT_EQ(toolbar->GetBounds().Height(), changed.metrics.toolbarHeight);
    EXPECT_FLOAT_EQ(menu->GetBounds().Height(), changed.metrics.menuBarHeight);

    viewer.Close();
    viewer.Open(sky::Uuid::Create());
    viewer.SynchronizeLayout();
    viewer.ArrangeIfNeeded(viewer.GetBounds());
    EXPECT_EQ(viewer.arrangements, 2);
    EXPECT_TRUE(viewer.IsOpen());

    const float oldRight = toolbar->GetBounds().right;
    const auto  dock     = viewer.GetChildren()[1]->GetBounds();
    auto        down     = Pointer(sky::ui::UIPointerAction::DOWN, dock.right - 1.0f, dock.bottom - 1.0f);
    viewer.OnPointerEvent(down);
    viewer.OnPointerEvent(Pointer(sky::ui::UIPointerAction::MOVE, down.x - 50.0f, down.y - 30.0f));
    EXPECT_TRUE(viewer.IsLayoutDirty());
    viewer.SynchronizeLayout();
    viewer.ArrangeIfNeeded(viewer.GetBounds());
    EXPECT_EQ(viewer.arrangements, 3);
    EXPECT_LT(toolbar->GetBounds().right, oldRight);
}

TEST(EditorShellTest, AssetBrowserEditsCookSetting)
{
    namespace fs = std::filesystem;

    const fs::path root = fs::temp_directory_path() / "sky_shell_asset_edit";
    fs::remove_all(root);
    fs::create_directories(root);
    std::ofstream(root / "a.txt") << "hello";

    auto *manager = sky::AssetBuilderManager::Get();
    auto *builder = new ShellTextBuilder();
    manager->RegisterBuilder(builder);

    sky::CookConfig cookConfig;
    cookConfig.Parse(R"({"bundles":["common"],"presets":{"windows":["common"]}})");
    cookConfig.SetActivePlatform("windows");
    manager->SetCookConfig(std::move(cookConfig));

    sky::FileSystemPtr workSpace = new sky::NativeFileSystem(sky::FilePath(root.string()));
    auto              *db        = sky::AssetDataBase::Get();
    db->Reset();
    db->SetWorkSpaceFs(workSpace);
    EXPECT_FALSE(db->RebuildCacheFromScan().has_value());

    auto *catalog = EditorAssetCatalog::Get();
    catalog->Refresh();

    AssetBrowserPanel panel;
    panel.SetCatalog(catalog);
    panel.SetBounds(sky::ui::UIRect{0.0f, 0.0f, 900.0f, 220.0f}); // short, like the bottom dock

    sky::ui::UIPaintContext ctx;
    ctx.Begin(sky::ui::UIRect{0.0f, 0.0f, 900.0f, 220.0f});
    panel.OnPaint(ctx); // populates the folder/item caches

    // Select the first item tile.
    panel.OnPointerEvent(Pointer(sky::ui::UIPointerAction::UP, 430.0f, 60.0f));

    EditorAssetItem item;
    ASSERT_TRUE(catalog->FindByPath("Project/a.txt", item));

    const auto infos  = catalog->GetTargetInfos(item.uuid);
    const auto target = infos.empty() ? std::string("common") : infos.front().target;

    // Edit through the reflected object and persist (what a committed form edit does).
    auto settings = catalog->GetCookSettings(item.uuid, target);
    ASSERT_TRUE(settings.IsValid());
    ASSERT_NE(settings.object.GetAsConst<ShellCookSettings>(), nullptr);
    settings.object.GetAs<ShellCookSettings>()->maxSize = 512;
    ASSERT_TRUE(catalog->ApplyCookSettings(item.uuid, target, settings.object));
    EXPECT_NE(db->GetCookJson(item.uuid).find("512"), std::string::npos);

    // Reset to the preset default removes the override key.
    settings                                            = catalog->GetCookSettings(item.uuid, target);
    settings.object.GetAs<ShellCookSettings>()->maxSize = 0;
    ASSERT_TRUE(catalog->ApplyCookSettings(item.uuid, target, settings.object));
    EXPECT_EQ(db->GetCookJson(item.uuid).find("512"), std::string::npos);

    manager->SetCookConfig(sky::CookConfig{});
    manager->UnRegisterBuilder(builder);
    db->Reset();
    db->SetWorkSpaceFs({});
    fs::remove_all(root);
}

TEST(EditorShellTest, AssetViewerOpensAndCloses)
{
    LayoutModel layout;
    layout.SetDefault({"outliner"});
    PanelRegistry registry;
    RegisterDefaultEditorPanels(registry);

    EditorShell shell;
    shell.SetLayout(&layout);
    shell.SetPanelRegistry(&registry);
    shell.SetAssetCatalog(EditorAssetCatalog::Get());
    shell.RegisterBuiltinPanelViews();
    shell.Rebuild();

    EXPECT_FALSE(shell.IsAssetViewerOpen());

    shell.OpenAssetViewer(sky::Uuid::Create());
    EXPECT_TRUE(shell.IsAssetViewerOpen());

    // Escape is routed to the active modal and closes the viewer (via the scoped action).
    shell.DispatchKey(Key(0x1B));
    EXPECT_FALSE(shell.IsAssetViewerOpen());
    EditorActionRegistry::Get()->Clear();
}

namespace {
    struct DocumentProbe {
        bool                     dirty = true, pending = false, fail = true, editing = false;
        int                      writes = 0, contentReleased = 0, documentReleased = 0, leaseReleased = 0;
        int                      cancelledEdits = 0, pointerEvents = 0, textEvents = 0;
        sky::ui::UIElement      *content = nullptr;
        std::vector<std::string> releaseOrder;
        CommandService           history;
    };
    class ProbeContent final : public sky::ui::UIElement {
    public:
        explicit ProbeContent(std::shared_ptr<DocumentProbe> state) : state(std::move(state))
        {
        }
        ~ProbeContent() override
        {
            ++state->contentReleased;
            state->releaseOrder.push_back("content");
        }
        sky::ui::UIEventResult OnPointerEvent(const sky::ui::UIPointerEvent &) override
        {
            ++state->pointerEvents;
            return sky::ui::UIEventResult::HANDLED;
        }
        sky::ui::UIEventResult OnTextInput(const sky::ui::UITextInputEvent &) override
        {
            ++state->textEvents;
            return sky::ui::UIEventResult::HANDLED;
        }

    private:
        std::shared_ptr<DocumentProbe> state;
    };
    class ProbeDocument final : public IAssetViewDocument {
    public:
        explicit ProbeDocument(std::shared_ptr<DocumentProbe> state) : state(std::move(state))
        {
        }
        ~ProbeDocument() override
        {
            ++state->documentReleased;
            state->releaseOrder.push_back("document");
        }
        std::unique_ptr<sky::ui::UIElement> CreateContent(sky::ui::UITextSystem *) override
        {
            auto content   = std::make_unique<ProbeContent>(state);
            state->content = content.get();
            return content;
        }
        CommandService &History() override
        {
            return state->history;
        }
        bool IsDirty() const override
        {
            return state->dirty;
        }
        bool IsEditing() const override
        {
            return state->editing;
        }
        void CancelInteractions() override
        {
            if (state->editing)
                ++state->cancelledEdits;
            state->editing = false;
        }
        void RequestSave() override
        {
            state->pending = true;
        }
        bool IsSavePending() const override
        {
            return state->pending;
        }
        std::string Status() const override
        {
            return state->fail ? "save failed" : "saved";
        }
        void Tick(float) override
        {
            if (state->pending) {
                ++state->writes;
                state->pending = false;
                if (!state->fail)
                    state->dirty = false;
            }
        }

    private:
        std::shared_ptr<DocumentProbe> state;
    };
    class ProbeDocumentProvider final : public IAssetViewProvider {
    public:
        bool UsesDocumentWindow() const override
        {
            return useWindows;
        }
        AssetViewDocument OpenDocument(const sky::Uuid &asset, const std::string &) override
        {
            state            = std::make_shared<DocumentProbe>();
            documents[asset] = state;
            auto lease       = std::shared_ptr<const void>(new int(0), [probe = state](const void *value) {
                delete static_cast<const int *>(value);
                ++probe->leaseReleased;
                probe->releaseOrder.push_back("lease");
            });
            return {std::move(lease), std::make_unique<ProbeDocument>(state)};
        }
        std::shared_ptr<DocumentProbe>                                state;
        std::unordered_map<sky::Uuid, std::shared_ptr<DocumentProbe>> documents;
        bool                                                          useWindows = false;
    };
} // namespace

TEST(EditorShellTest, AssetDocumentCloseAndReplacementWaitForDecisionAndRetainCodeThroughDestruction)
{
    ProbeDocumentProvider        provider;
    ScopedAssetViewerEnvironment environment;
    std::filesystem::create_directories(environment.root);
    std::ofstream(environment.root / "a.txt") << "a";
    std::ofstream(environment.root / "b.txt") << "b";
    if (environment.manager->QueryBuilder(".txt") == nullptr) {
        environment.temporaryBuilder = new ShellTextBuilder();
        environment.manager->RegisterBuilder(environment.temporaryBuilder);
    }
    sky::CookConfig config;
    ASSERT_TRUE(config.Parse(R"({"bundles":["common"],"presets":{"windows":["common"]}})"));
    config.SetActivePlatform("windows");
    environment.manager->SetCookConfig(std::move(config));
    environment.databaseChanged = true;
    environment.database->Reset();
    environment.database->SetWorkSpaceFs(new sky::NativeFileSystem(sky::FilePath(environment.root.string())));
    ASSERT_FALSE(environment.database->RebuildCacheFromScan().has_value());
    environment.catalog->Refresh();
    environment.providers->Register("Text", &provider);
    EditorAssetItem a, b;
    ASSERT_TRUE(environment.catalog->FindByPath("Project/a.txt", a));
    ASSERT_TRUE(environment.catalog->FindByPath("Project/b.txt", b));
    AssetViewerWidget viewer(nullptr);
    viewer.SetCatalog(environment.catalog);
    viewer.Open(a.uuid);
    auto first = provider.state;
    ASSERT_NE(first, nullptr);
    viewer.SaveAsset();
    EXPECT_EQ(first->writes, 0);
    viewer.Tick(0);
    EXPECT_EQ(first->writes, 1);
    viewer.Close();
    viewer.Tick(0);
    EXPECT_TRUE(viewer.IsOpen());
    EXPECT_TRUE(viewer.NeedsCloseDecision());
    const auto prompt = std::find_if(viewer.GetChildren().begin(), viewer.GetChildren().end(),
                                     [](const auto &child) { return std::string(child->GetTypeName()) == "AssetClosePrompt"; });
    ASSERT_NE(prompt, viewer.GetChildren().end());
    EXPECT_TRUE((*prompt)->IsVisible());
    viewer.ResolveClose(AssetViewerWidget::CloseDecision::Cancel);
    viewer.Tick(0);
    EXPECT_FALSE(viewer.NeedsCloseDecision());
    EXPECT_FALSE((*prompt)->IsVisible());
    EXPECT_EQ(first->contentReleased, 0);
    viewer.Close();
    viewer.ResolveClose(AssetViewerWidget::CloseDecision::Save);
    EXPECT_FALSE(viewer.CanResolveClose(AssetViewerWidget::CloseDecision::Save));
    EXPECT_FALSE(viewer.CanResolveClose(AssetViewerWidget::CloseDecision::Discard));
    EXPECT_TRUE(viewer.CanResolveClose(AssetViewerWidget::CloseDecision::Cancel));
    viewer.Tick(0);
    EXPECT_TRUE(viewer.IsOpen());
    EXPECT_EQ(first->documentReleased, 0);
    EXPECT_TRUE((*prompt)->IsVisible());
    first->fail = false;
    viewer.ResolveClose(AssetViewerWidget::CloseDecision::Save);
    EXPECT_EQ(first->writes, 2);
    viewer.Tick(0);
    EXPECT_FALSE(viewer.IsOpen());
    EXPECT_EQ(first->releaseOrder, (std::vector<std::string>{"content", "document", "lease"}));
    viewer.Open(a.uuid);
    auto second = provider.state;
    viewer.Open(b.uuid);
    EXPECT_EQ(viewer.AssetId(), a.uuid);
    viewer.Tick(0);
    EXPECT_EQ(viewer.AssetId(), a.uuid);
    second->editing = true;
    EXPECT_FALSE(viewer.CanResolveClose(AssetViewerWidget::CloseDecision::Save));
    viewer.ResolveClose(AssetViewerWidget::CloseDecision::Save);
    EXPECT_FALSE(second->pending);
    viewer.ResolveClose(AssetViewerWidget::CloseDecision::Discard);
    viewer.Tick(0);
    EXPECT_TRUE(viewer.IsOpen());
    EXPECT_EQ(viewer.AssetId(), b.uuid);
    EXPECT_EQ(second->cancelledEdits, 1);
    EXPECT_EQ(second->releaseOrder, (std::vector<std::string>{"content", "document", "lease"}));
    provider.state->dirty   = false;
    provider.state->editing = true;
    viewer.Close();
    viewer.Tick(0);
    EXPECT_TRUE(viewer.NeedsCloseDecision());
    viewer.ResolveClose(AssetViewerWidget::CloseDecision::Cancel);
    viewer.Tick(0);
    EXPECT_TRUE(provider.state->editing);
    EXPECT_TRUE(viewer.IsOpen());
    viewer.Close();
    viewer.ResolveClose(AssetViewerWidget::CloseDecision::Discard);
    viewer.Tick(0);
    EXPECT_FALSE(viewer.IsOpen());
    EXPECT_EQ(provider.state->cancelledEdits, 1);
}

TEST(EditorShellTest, AssetToolWindowsKeepActionsDocumentsAndCloseDecisionsIndependent)
{
    ProbeDocumentProvider provider;
    provider.useWindows = true;
    ScopedAssetViewerEnvironment environment;
    std::filesystem::create_directories(environment.root);
    std::ofstream(environment.root / "first.txt") << "first";
    std::ofstream(environment.root / "second.txt") << "second";
    if (environment.manager->QueryBuilder(".txt") == nullptr) {
        environment.temporaryBuilder = new ShellTextBuilder();
        environment.manager->RegisterBuilder(environment.temporaryBuilder);
    }
    environment.databaseChanged = true;
    environment.database->Reset();
    environment.database->SetWorkSpaceFs(new sky::NativeFileSystem(sky::FilePath(environment.root.string())));
    ASSERT_FALSE(environment.database->RebuildCacheFromScan().has_value());
    environment.catalog->Refresh();
    environment.providers->Register("Text", &provider);
    EditorAssetItem first, second;
    ASSERT_TRUE(environment.catalog->FindByPath("Project/first.txt", first));
    ASSERT_TRUE(environment.catalog->FindByPath("Project/second.txt", second));
    EditorShell shell;
    shell.SetAssetCatalog(environment.catalog);
    uint32_t              nextSurface = 100;
    std::vector<uint32_t> retired;
    shell.SetAssetToolHost([&](const std::string &) { return nextSurface++; }, [&](uint32_t id) { retired.push_back(id); });
    shell.OpenAssetViewer(first.uuid);
    shell.OpenAssetViewer(second.uuid);
    EXPECT_TRUE(provider.documents.empty());
    shell.TickAssetViews(0);
    ASSERT_EQ(provider.documents.size(), 2u);
    auto                    a = provider.documents.at(first.uuid), b = provider.documents.at(second.uuid);
    sky::ui::UIPaintContext paint;
    shell.PaintSurface(100, paint, 900, 650);
    shell.PaintSurface(101, paint, 1100, 750);
    EXPECT_TRUE(EditorScopeStack::Get()->IsEmpty());
    const auto invoke = [&](const sky::Uuid &asset, const char *action) {
        ScopedEditorActions scope("assetTool." + asset.ToString());
        return EditorActionRegistry::Get()->Invoke(action);
    };
    auto save      = Key('S');
    save.modifiers = kModCtrl;
    ASSERT_TRUE(shell.DispatchKeyToSurface(100, save));
    EXPECT_EQ(a->writes, 0);
    shell.TickAssetViews(0);
    EXPECT_EQ(a->writes, 1);
    EXPECT_EQ(b->writes, 0);
    class ProbeEdit final : public UndoCommand {
    public:
        explicit ProbeEdit(int &value) : UndoCommand("Probe edit"), value(value)
        {
        }
        void Do() override
        {
            value = 1;
        }
        void Undo() override
        {
            value = 0;
        }

    private:
        int &value;
    };
    int firstValue = 0, secondValue = 0;
    a->history.Execute(std::make_unique<ProbeEdit>(firstValue));
    b->history.Execute(std::make_unique<ProbeEdit>(secondValue));
    auto undo      = Key('Z');
    undo.modifiers = kModCtrl;
    ASSERT_TRUE(shell.DispatchKeyToSurface(100, undo));
    EXPECT_EQ(firstValue, 0);
    EXPECT_EQ(secondValue, 1);
    auto redo      = Key('Y');
    redo.modifiers = kModCtrl;
    ASSERT_TRUE(shell.DispatchKeyToSurface(100, redo));
    EXPECT_EQ(firstValue, 1);
    ASSERT_TRUE(shell.DispatchKeyToSurface(101, undo));
    EXPECT_EQ(firstValue, 1);
    EXPECT_EQ(secondValue, 0);
    shell.OpenAssetViewer(first.uuid);
    shell.TickAssetViews(0);
    EXPECT_EQ(nextSurface, 102u);
    EXPECT_TRUE(shell.RequestCloseAssetTool(100));
    shell.TickAssetViews(0);
    EXPECT_TRUE(retired.empty());
    ASSERT_TRUE(shell.DispatchKeyToSurface(100, undo));
    EXPECT_EQ(firstValue, 1);
    ASSERT_TRUE(shell.DispatchKeyToSurface(100, save));
    EXPECT_FALSE(a->pending);
    ASSERT_TRUE(shell.DispatchTextToSurface(100, {}));
    EXPECT_EQ(a->textEvents, 0);
    ASSERT_TRUE(shell.DispatchKeyToSurface(100, Key(0x1B)));
    EXPECT_FALSE(shell.AreAssetViewsClosing());
    EXPECT_EQ(a->documentReleased, 0);
    shell.RequestCloseAssetViews();
    EXPECT_TRUE(shell.AreAssetViewsClosing());
    shell.TickAssetViews(0);
    shell.PaintSurface(100, paint, 900, 650);
    auto      *viewerRoot = a->content->GetParent()->GetParent();
    const auto prompt     = std::find_if(viewerRoot->GetChildren().begin(), viewerRoot->GetChildren().end(),
                                         [](const auto &child) { return std::string(child->GetTypeName()) == "AssetClosePrompt"; });
    ASSERT_NE(prompt, viewerRoot->GetChildren().end());
    ASSERT_TRUE((*prompt)->IsVisible());
    ASSERT_EQ((*prompt)->GetChildren().size(), 3u);
    const auto contentBounds = a->content->GetBounds();
    EXPECT_TRUE(shell.DispatchPointerToSurface(100, Pointer(sky::ui::UIPointerAction::DOWN, contentBounds.right - 1, contentBounds.top + 1)));
    EXPECT_TRUE(shell.DispatchPointerToSurface(100, Pointer(sky::ui::UIPointerAction::UP, contentBounds.right - 1, contentBounds.top + 1)));
    EXPECT_EQ(a->pointerEvents, 0);
    const auto discard = (*prompt)->GetChildren()[1]->GetBounds();
    EXPECT_TRUE(shell.DispatchPointerToSurface(100, Pointer(sky::ui::UIPointerAction::DOWN, CenterX(discard), CenterY(discard))));
    EXPECT_TRUE(shell.DispatchPointerToSurface(100, Pointer(sky::ui::UIPointerAction::UP, CenterX(discard), CenterY(discard))));
    EXPECT_EQ(a->documentReleased, 0);
    EXPECT_TRUE(shell.DispatchKeyToSurface(101, Key(0x09)));
    EXPECT_TRUE(shell.DispatchKeyToSurface(101, Key(0x0D)));
    shell.TickAssetViews(0);
    EXPECT_EQ(retired, (std::vector<uint32_t>{100}));
    EXPECT_EQ(a->releaseOrder, (std::vector<std::string>{"content", "document", "lease"}));
    EXPECT_TRUE(shell.IsAssetViewerOpen());
    EXPECT_EQ(b->documentReleased, 0);
    b->fail = false;
    EXPECT_TRUE(invoke(second.uuid, "assetViewer.saveClose"));
    shell.TickAssetViews(0);
    EXPECT_EQ(retired, (std::vector<uint32_t>{100, 101}));
    EXPECT_FALSE(shell.IsAssetViewerOpen());
    EXPECT_EQ(b->releaseOrder, (std::vector<std::string>{"content", "document", "lease"}));
    EXPECT_FALSE(invoke(first.uuid, "assetViewer.save"));
}

TEST(EditorShellTest, ReflectionDemoPanelsConsumeProcessMetadata)
{
    auto       *context = sky::SerializationContext::Get();
    const auto *node    = context->FindType("ReflectionDemoObject");
    ASSERT_NE(node, nullptr);
    auto first  = sky::editor::CreateReflectionDemoPanel(nullptr, "First");
    auto second = sky::editor::CreateReflectionDemoPanel(nullptr, "Second");
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    EXPECT_EQ(context->FindType("ReflectionDemoObject"), node);
    first.reset();
    auto reopened = sky::editor::CreateReflectionDemoPanel(nullptr, "Reopened");
    ASSERT_NE(reopened, nullptr);
    EXPECT_EQ(context->FindType("ReflectionDemoObject"), node);
}

TEST(EditorShellTest, TwoTimesAssetBrowserUsesThemeTextAndMatchingTreeAndTileHits)
{
    namespace fs = std::filesystem;
    struct TestState {
        UiTheme             previous = GetDefaultUiTheme();
        fs::path            root     = fs::temp_directory_path() / ("sky_asset_browser_scale_" + sky::Uuid::Create().ToString());
        sky::AssetDataBase *database = sky::AssetDataBase::Get();
        ShellTextBuilder   *builder  = new ShellTextBuilder();
        ~TestState()
        {
            database->Reset();
            database->SetWorkSpaceFs({});
            EditorAssetCatalog::Get()->Refresh();
            sky::AssetBuilderManager::Get()->UnRegisterBuilder(builder);
            std::error_code error;
            fs::remove_all(root, error);
            SetDefaultUiTheme(previous);
        }
    } state;
    class RecordingFont : public sky::ui::IUIFontProvider {
    public:
        bool GetGlyph(uint32_t codepoint, uint32_t size, sky::ui::UIGlyphBitmap &glyph) override
        {
            sizes.push_back(size);
            return builtin.GetGlyph(codepoint, size, glyph);
        }
        sky::ui::UIFontMetrics GetFontMetrics(uint32_t size) override
        {
            return builtin.GetFontMetrics(size);
        }
        std::vector<uint32_t>          sizes;
        sky::ui::UIBuiltinFontProvider builtin;
    } font;

    sky::AssetBuilderManager::Get()->RegisterBuilder(state.builder);
    std::error_code error;
    fs::create_directories(state.root / "one", error);
    ASSERT_FALSE(error);
    fs::create_directories(state.root / "two", error);
    ASSERT_FALSE(error);
    for (const char *path : {"one/a.txt", "one/b.txt", "two/c.txt"}) {
        std::ofstream file(state.root / path);
        file << "asset";
        ASSERT_TRUE(file.good());
    }
    state.database->Reset();
    state.database->SetWorkSpaceFs(new sky::NativeFileSystem(sky::FilePath(state.root.string())));
    ASSERT_FALSE(state.database->RebuildCacheFromScan().has_value());
    auto *catalog = EditorAssetCatalog::Get();
    catalog->Refresh();
    const auto one = catalog->ListFolder("Project/one");
    const auto two = catalog->ListFolder("Project/two");
    ASSERT_EQ(one.items.size(), 2u);
    ASSERT_EQ(two.items.size(), 1u);

    const UiTheme theme = MakeDarkTheme(2.0f);
    SetDefaultUiTheme(theme);
    const auto           &m = theme.metrics;
    sky::ui::UITextSystem text(&font, nullptr);
    SelectionService      selection;
    AssetBrowserPanel     panel;
    panel.SetTitleBarVisible(false);
    panel.SetCatalog(catalog);
    panel.SetTextSystem(&text);
    panel.SetSelection(&selection);
    std::vector<sky::Uuid> opened;
    panel.SetOpenHandler([&](const sky::Uuid &id) { opened.push_back(id); });
    const sky::ui::UIRect bounds{50, 80, 1650, 380};
    panel.Arrange(bounds);
    sky::ui::UIPaintContext paint;
    const auto              render = [&]() {
        paint.Begin({0, 0, 1800, 1400});
        panel.OnPaint(paint);
    };
    render();
    ASSERT_FALSE(font.sizes.empty());
    EXPECT_TRUE(std::all_of(font.sizes.begin(), font.sizes.end(), [&](uint32_t size) { return size == theme.fonts.label; }));

    const auto treeRow = [&](int index) {
        return Pointer(sky::ui::UIPointerAction::UP, bounds.left + m.cellPadding,
                       bounds.top + m.listHeaderHeight + m.checkboxPad + (static_cast<float>(index) + 0.5f) * m.rowHeight);
    };
    EXPECT_EQ(panel.OnPointerEvent(treeRow(1)), sky::ui::UIEventResult::HANDLED);
    render();
    const float tileLeft  = bounds.left + bounds.Width() * 0.42f + m.checkboxPad + m.itemSpacing;
    const float tileTop   = bounds.top + m.listHeaderHeight + m.itemSpacing;
    const auto  tileClick = Pointer(sky::ui::UIPointerAction::UP, tileLeft + m.assetTileSize - m.checkboxPad, tileTop + m.assetTileSize * 0.5f);
    EXPECT_EQ(panel.OnPointerEvent(tileClick), sky::ui::UIEventResult::HANDLED);
    ASSERT_EQ(selection.GetSelection().size(), 1u);
    EXPECT_EQ(selection.GetSelection().front().id, one.items.front().uuid);
    render();
    EXPECT_EQ(panel.OnPointerEvent(tileClick), sky::ui::UIEventResult::HANDLED);
    ASSERT_EQ(opened.size(), 1u);
    EXPECT_EQ(opened.front(), one.items.front().uuid);
    EXPECT_EQ(panel.OnPointerEvent(Pointer(sky::ui::UIPointerAction::UP, tileLeft + m.assetTileSize + m.itemSpacing * 0.5f, tileClick.y)),
              sky::ui::UIEventResult::UNHANDLED);
    EXPECT_EQ(selection.GetSelection().front().id, one.items.front().uuid);

    EXPECT_EQ(panel.OnPointerEvent(treeRow(2)), sky::ui::UIEventResult::HANDLED);
    render();
    EXPECT_EQ(panel.OnPointerEvent(tileClick), sky::ui::UIEventResult::HANDLED);
    ASSERT_EQ(selection.GetSelection().size(), 1u);
    EXPECT_EQ(selection.GetSelection().front().id, two.items.front().uuid);
}
