//
// Created on 2026/10/06.
//

#include <core/type/TypeInfo.h>
#include <core/type/TypeInfoObj.h>
#include <cstdlib>
#include <editor/core/asset/EditorAssetCatalog.h>
#include <editor/core/document/WorldDocument.h>
#include <editor/core/extension/EditorActionRegistry.h>
#include <editor/core/filebrowser/FileBrowserModel.h>
#include <editor/core/layout/DefaultPanels.h>
#include <editor/core/layout/LayoutModel.h>
#include <editor/core/layout/PanelRegistry.h>
#include <editor/core/preferences/PreferenceRegistry.h>
#include <editor/core/preferences/PreferenceStore.h>
#include <editor/shell/EditorShell.h>
#include <editor/shell/FileBrowserDialog.h>
#include <editor/shell/ReflectedFormView.h>
#include <editor/shell/UiTheme.h>
#include <editor/shell/WorldConfigPanel.h>
#include <editor/shell/panels/AssetBrowserPanel.h>
#include <editor/shell/widgets/ToolBar.h>
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

    PreferenceRegistry pref;
    PreferenceSection  sectionA;
    sectionA.id    = "a.main";
    sectionA.title = "A";
    sectionA.entries.push_back({"a.enabled", "Enabled", PreferenceValue::Bool(true)});
    PreferencePage pageA;
    pageA.id       = "a";
    pageA.title    = "A";
    pageA.sections = {sectionA};
    pref.RegisterPage(pageA);

    PreferenceSection sectionB;
    sectionB.id    = "b.main";
    sectionB.title = "B";
    sectionB.entries.push_back({"b.count", "Count", PreferenceValue::Int(1), 0.0, 10.0});
    PreferencePage pageB;
    pageB.id       = "b";
    pageB.title    = "B";
    pageB.sections = {sectionB};
    pref.RegisterPage(pageB);

    PreferenceStore store(&pref);
    shell.SetPreferences(&pref, &store, []() {});

    EXPECT_FALSE(shell.IsPreferencesOpen());
    shell.OpenPreferences();
    EXPECT_TRUE(shell.IsPreferencesOpen());
    EXPECT_TRUE(shell.WantsInput());

    sky::ui::UIKeyEvent esc;
    esc.keyCode = 0x1B;
    esc.action  = sky::ui::UIKeyAction::DOWN;
    EXPECT_TRUE(shell.DispatchKey(esc));
    EXPECT_FALSE(shell.IsPreferencesOpen());
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
    WorldConfigPanel panel(
        nullptr, [&document]() { return &document; }, "Config");
    panel.SetBounds(sky::ui::UIRect{0.0f, 0.0f, 300.0f, 400.0f});

    // Subsystems are enabled by default; clicking the row's checkbox disables it
    // and persists the selection.
    sky::ui::UIPointerEvent down;
    down.action = sky::ui::UIPointerAction::DOWN;
    down.x      = 10.0f;
    down.y      = 41.0f; // first row's checkbox
    panel.OnPointerEvent(down);

    bool enabled = true;
    ASSERT_TRUE(document.IsSubSystemEnabled("Test", enabled));
    EXPECT_FALSE(enabled);

    std::filesystem::remove(path, ec);
    sky::WorldSubSystemRegistry::Get().Clear();
}

TEST(EditorShellTest, ConfigPanelKeepsConfigValidAfterToggleRealloc)
{
    test::RegisterTestTypes();
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
    WorldConfigPanel panel(
        nullptr, [&document]() { return &document; }, "Config");
    panel.SetBounds(sky::ui::UIRect{0.0f, 0.0f, 300.0f, 400.0f});

    ReflectedFormView *form = nullptr;
    for (const auto &child : panel.GetChildren()) {
        if (auto *view = dynamic_cast<ReflectedFormView *>(child.get())) {
            form = view;
        }
    }
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
    EditorActionRegistry::Get()->Add({.id = "file.save", .label = "Save", .invoke = [&saves]() { ++saves; }});

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
    EditorActionRegistry::Get()->Add({.id = "file.close", .label = "Close", .invoke = [&closes]() { ++closes; }});

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
    test::RegisterTestTypes();
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

    class ShellTextBuilder : public sky::AssetBuilder {
    public:
        ShellTextBuilder()
        {
            static bool registered = false;
            if (!registered) {
                registered = true;
                sky::SerializationContext::Get()->Register<ShellCookSettings>("ShellTestCookSettings").Member<&ShellCookSettings::maxSize>("maxSize");
            }
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

} // namespace

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
    db->RebuildCacheFromScan();

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

    // Escape is routed to the active modal and closes the viewer.
    shell.DispatchKey(Key(0x1B));
    EXPECT_FALSE(shell.IsAssetViewerOpen());
}
