//
// Created on 2026/10/06.
//

#include <editor/core/filebrowser/FileBrowserModel.h>
#include <editor/core/layout/LayoutModel.h>
#include <editor/core/layout/PanelRegistry.h>
#include <editor/core/preferences/PreferenceRegistry.h>
#include <editor/core/preferences/PreferenceStore.h>
#include <editor/shell/EditorShell.h>
#include <editor/shell/FileBrowserDialog.h>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <ui/UIElement.h>

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
