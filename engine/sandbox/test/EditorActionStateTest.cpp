#include <editor/core/extension/EditorActionRegistry.h>
#include <editor/core/extension/EditorScopeStack.h>
#include <editor/core/extension/IconRegistry.h>
#include <editor/core/input/KeyModifiers.h>
#include <editor/shell/AssetViewerWidget.h>
#include <editor/shell/EditorShell.h>
#include <editor/ui/UiTheme.h>
#include <editor/widgets/ActionBarBuilder.h>
#include <editor/widgets/MenuBar.h>
#include <editor/widgets/ToolBar.h>
#include <gtest/gtest.h>
#include <ui/UIPaintContext.h>

#include <algorithm>
#include <memory>

namespace sky::editor::action_state_test {
    namespace {
        class EditorActionStateTest : public testing::Test {
        protected:
            void SetUp() override
            {
                EditorActionRegistry::Get()->Clear();
                EditorScopeStack::Get()->Clear();
                SetDefaultUiTheme(MakeDarkTheme());
            }
            void TearDown() override
            {
                EditorActionRegistry::Get()->Clear();
                EditorScopeStack::Get()->Clear();
            }
        };

        ui::UIPointerEvent Down(float x, float y)
        {
            ui::UIPointerEvent event;
            event.action = ui::UIPointerAction::DOWN;
            event.x      = x;
            event.y      = y;
            return event;
        }
        ui::UIKeyEvent Shortcut()
        {
            ui::UIKeyEvent key;
            key.keyCode   = 'K';
            key.modifiers = kModCtrl;
            return key;
        }
        EditorAction Action(int &invoked)
        {
            EditorAction action;
            action.id          = "state.test";
            action.label       = "Test";
            action.icon        = "action-state-icon";
            action.group       = EditorGroup::History;
            action.menu        = EditorMenu::Edit;
            action.accelerator = "Ctrl+K";
            action.invoke      = [&invoked]() { ++invoked; };
            return action;
        }
        bool HasTexture(const ui::UIDrawData &data, uint32_t id)
        {
            return std::any_of(data.commands.begin(), data.commands.end(), [id](const auto &cmd) { return cmd.textureId == id; });
        }
        bool TextureHasColor(const ui::UIDrawData &data, uint32_t id, uint32_t color)
        {
            for (const auto &cmd : data.commands) {
                if (cmd.textureId != id)
                    continue;
                for (uint32_t i = 0; i < cmd.indexCount; ++i) {
                    if (data.vertices[data.indices[cmd.indexOffset + i]].color == color)
                        return true;
                }
            }
            return false;
        }
        bool HasColor(const ui::UIDrawData &data, uint32_t color)
        {
            return std::any_of(data.vertices.begin(), data.vertices.end(), [color](const auto &v) { return v.color == color; });
        }
        bool HasColorInRect(const ui::UIDrawData &data, uint32_t color, const ui::UIRect &rect)
        {
            return std::any_of(data.vertices.begin(), data.vertices.end(),
                               [color, rect](const auto &v) { return v.color == color && rect.Contains(v.x, v.y); });
        }
        void OpenMenu(MenuBar &menu)
        {
            const auto &m = GetDefaultUiTheme().metrics;
            const auto  b = menu.GetBounds();
            EXPECT_EQ(menu.OnPointerEvent(Down(b.left + m.padX + m.controlPad, b.top + m.menuBarHeight * 0.5f)), ui::UIEventResult::HANDLED);
            EXPECT_TRUE(menu.IsOpen());
        }
        ui::UIPointerEvent MenuRow(const MenuBar &menu, size_t row = 0)
        {
            const auto &m = GetDefaultUiTheme().metrics;
            const auto  b = menu.GetBounds();
            return Down(b.left + m.padX + m.controlPad, b.top + m.menuBarHeight + m.panelGap + m.popupItemHeight * (static_cast<float>(row) + 0.5f));
        }
        ToolBar *FindToolBar(ui::UIElement &root)
        {
            if (auto *bar = dynamic_cast<ToolBar *>(&root))
                return bar;
            for (const auto &child : root.GetChildren())
                if (auto *bar = FindToolBar(*child))
                    return bar;
            return nullptr;
        }
        MenuBar *FindMenuBar(ui::UIElement &root)
        {
            if (auto *bar = dynamic_cast<MenuBar *>(&root))
                return bar;
            for (const auto &child : root.GetChildren())
                if (auto *bar = FindMenuBar(*child))
                    return bar;
            return nullptr;
        }
    } // namespace

    TEST_F(EditorActionStateTest, RegistryDefaultsAndSharedStateControlAllDispatchPaths)
    {
        int  invoked = 0;
        bool enabled = true, visible = true, checked = false;
        auto action      = Action(invoked);
        action.enabled   = [&]() { return enabled; };
        action.visible   = [&]() { return visible; };
        action.checkable = true;
        action.checked   = [&]() { return checked; };
        auto *registry   = EditorActionRegistry::Get();
        registry->Add(action);
        EditorShell shell;
        const auto  invoke  = [&shell](const std::string &id) { shell.InvokeAction(id); };
        const auto  toolbar = ActionBarBuilder::BuildToolBarItems(invoke);
        const auto  menus   = ActionBarBuilder::BuildMenus(invoke);
        ASSERT_EQ(toolbar.size(), 1u);
        ASSERT_EQ(menus.size(), 1u);
        EXPECT_TRUE(toolbar.front().enabled);
        EXPECT_TRUE(toolbar.front().checkable);
        EXPECT_FALSE(menus.front().items.front().checked);
        toolbar.front().action();
        menus.front().items.front().action();
        EXPECT_TRUE(shell.DispatchKey(Shortcut()));
        EXPECT_EQ(invoked, 3);
        enabled = false;
        checked = true;
        EXPECT_TRUE(registry->ResolveState(registry->GetSnapshot(action.id)).checked);
        toolbar.front().action();
        menus.front().items.front().action();
        EXPECT_EQ(ActionBarBuilder::MatchAccelerator(Shortcut()), action.id);
        EXPECT_TRUE(shell.DispatchKey(Shortcut()));
        EXPECT_EQ(invoked, 3);
        visible = false;
        EXPECT_TRUE(ActionBarBuilder::MatchAccelerator(Shortcut()).empty());
        EXPECT_FALSE(shell.InvokeAction(action.id));
        toolbar.front().action();
        menus.front().items.front().action();
        EXPECT_EQ(invoked, 3);
    }

    TEST_F(EditorActionStateTest, MutatingEveryStatePredicateRejectsBothOldAndReplacementHandlers)
    {
        auto *registry = EditorActionRegistry::Get();
        for (int predicate = 0; predicate != 3; ++predicate) {
            SCOPED_TRACE(predicate);
            int        oldCalls = 0, replacementCalls = 0;
            auto       replacement = Action(replacementCalls);
            auto       action      = Action(oldCalls);
            const auto replace     = [registry, replacement]() {
                registry->Add(replacement);
                return true;
            };
            if (predicate == 0)
                action.visible = replace;
            if (predicate == 1)
                action.enabled = replace;
            if (predicate == 2) {
                action.checkable = true;
                action.checked   = replace;
            }
            registry->Add(action);
            EXPECT_FALSE(registry->Invoke(action.id));
            EXPECT_EQ(oldCalls, 0);
            EXPECT_EQ(replacementCalls, 0);
            EXPECT_TRUE(registry->Invoke(action.id));
            EXPECT_EQ(replacementCalls, 1);
        }
        int  calls     = 0;
        auto action    = Action(calls);
        action.enabled = [registry]() {
            registry->Clear();
            return true;
        };
        registry->Add(action);
        EXPECT_FALSE(registry->Invoke(action.id));
        EXPECT_EQ(calls, 0);
    }

    TEST_F(EditorActionStateTest, RetainedChromeCannotInvokeReplacementsOrAnotherActiveScope)
    {
        auto *registry = EditorActionRegistry::Get();
        int   oldCalls = 0, newCalls = 0;
        auto  action = Action(oldCalls);
        registry->Add(action);
        const auto invoke   = [registry](const std::string &id) { registry->Invoke(id); };
        const auto items    = ActionBarBuilder::BuildToolBarItems(invoke);
        const auto menus    = ActionBarBuilder::BuildMenus(invoke);
        const auto snapshot = registry->GetSnapshot(action.id);
        registry->Remove("global", action.id);
        registry->Add(Action(newCalls));
        items.front().action();
        menus.front().items.front().action();
        EXPECT_FALSE(registry->Invoke(snapshot));
        EXPECT_EQ(oldCalls, 0);
        EXPECT_EQ(newCalls, 0);
        const auto current = ActionBarBuilder::BuildToolBarItems(invoke);
        EditorScopeStack::Get()->Push("other");
        current.front().action(); // even the existing global fallback is not an old chrome target
        EXPECT_EQ(newCalls, 0);
        action.scope = "other";
        registry->Add(action);
        current.front().action();
        EXPECT_EQ(oldCalls, 0);
        EXPECT_TRUE(registry->Invoke(action.id));
        EXPECT_EQ(oldCalls, 1);
    }

    TEST_F(EditorActionStateTest, SelfUnregisterAndScopeMutationAreSafeDuringDispatch)
    {
        int   calls    = 0;
        auto *registry = EditorActionRegistry::Get();
        auto  action   = Action(calls);
        action.invoke  = [&]() {
            ++calls;
            registry->Clear();
        };
        registry->Add(action);
        EXPECT_TRUE(registry->Invoke(action.id));
        EXPECT_EQ(calls, 1);
        action.enabled = []() {
            EditorScopeStack::Get()->Push("changed");
            return true;
        };
        registry->Add(action);
        EXPECT_FALSE(registry->Invoke(action.id));
        EXPECT_EQ(calls, 1);
    }

    TEST_F(EditorActionStateTest, ToolbarPaintAndHitGeometryFollowVisibleDisabledAndCheckedState)
    {
        constexpr uint32_t icon = 231;
        IconRegistry::Get()->Set("action-state-icon", icon);
        int  calls   = 0;
        bool enabled = false, visible = true, checked = false;
        auto action      = Action(calls);
        action.enabled   = [&]() { return enabled; };
        action.visible   = [&]() { return visible; };
        action.checkable = true;
        action.checked   = [&]() { return checked; };
        auto *registry   = EditorActionRegistry::Get();
        registry->Add(action);
        ToolBar bar(nullptr);
        bar.SetItems(ActionBarBuilder::BuildToolBarItems([registry](const std::string &id) { registry->Invoke(id); }));
        const auto &th = GetDefaultUiTheme();
        bar.Arrange({0, 0, 600, th.metrics.toolbarHeight});
        ui::UIPaintContext paint;
        paint.Begin(bar.GetBounds());
        bar.Paint(paint);
        EXPECT_TRUE(TextureHasColor(paint.GetDrawData(), icon, th.colors.textDisabled));
        EXPECT_EQ(bar.OnPointerEvent(Down(th.metrics.padX + th.metrics.iconButtonWidth * 0.5f, th.metrics.toolbarHeight * 0.5f)),
                  ui::UIEventResult::HANDLED);
        EXPECT_EQ(calls, 0);
        enabled = true;
        checked = true;
        EXPECT_TRUE(bar.RefreshActionStates());
        paint.Begin(bar.GetBounds());
        bar.Paint(paint);
        EXPECT_TRUE(TextureHasColor(paint.GetDrawData(), icon, th.colors.text));
        EXPECT_TRUE(HasColor(paint.GetDrawData(), th.colors.accentSoft));
        visible = false;
        EXPECT_TRUE(bar.RefreshActionStates());
        paint.Begin(bar.GetBounds());
        bar.Paint(paint);
        EXPECT_FALSE(HasTexture(paint.GetDrawData(), icon));
        EXPECT_EQ(bar.OnPointerEvent(Down(th.metrics.padX + 1, th.metrics.toolbarHeight * 0.5f)), ui::UIEventResult::UNHANDLED);
    }

    TEST_F(EditorActionStateTest, MenuRefreshPreservesPopupAndCompressesHiddenRows)
    {
        constexpr uint32_t icon = 232;
        IconRegistry::Get()->Set("action-state-icon", icon);
        int  calls   = 0;
        bool enabled = false, visible = true, checked = false;
        auto action      = Action(calls);
        action.enabled   = [&]() { return enabled; };
        action.visible   = [&]() { return visible; };
        action.checkable = true;
        action.checked   = [&]() { return checked; };
        auto *registry   = EditorActionRegistry::Get();
        registry->Add(action);
        int  tailCalls = 0;
        auto tail      = Action(tailCalls);
        tail.id        = "state.tail";
        tail.order     = 1;
        tail.icon.clear();
        registry->Add(tail);
        const auto  invoke = [registry](const std::string &id) { registry->Invoke(id); };
        MenuBar     menu(ActionBarBuilder::BuildMenus(invoke), nullptr);
        const auto &th = GetDefaultUiTheme();
        menu.SetBarHeight(th.metrics.menuBarHeight);
        menu.Arrange({0, 0, 600, 300});
        OpenMenu(menu);
        const float twoRows = menu.PopupHeight();
        menu.OnPointerEvent(MenuRow(menu));
        EXPECT_EQ(calls, 0);
        EXPECT_TRUE(menu.IsOpen());
        ui::UIPaintContext paint;
        const float        checkLeft    = th.metrics.padX + 1.0f + th.metrics.controlPad;
        const float        checkCenterY = th.metrics.menuBarHeight + th.metrics.panelGap + th.metrics.popupItemHeight * 0.5f;
        const ui::UIRect   checkRect{checkLeft, checkCenterY - th.metrics.iconSize * 0.5f, checkLeft + th.metrics.iconSize,
                                   checkCenterY + th.metrics.iconSize * 0.5f};
        paint.Begin(menu.GetBounds());
        menu.Paint(paint);
        EXPECT_TRUE(TextureHasColor(paint.GetDrawData(), icon, th.colors.textDisabled));
        EXPECT_FALSE(HasColorInRect(paint.GetDrawData(), th.colors.textOnAccent, checkRect));
        enabled = true;
        checked = true;
        EXPECT_TRUE(menu.RefreshActionStates());
        EXPECT_TRUE(menu.IsOpen());
        paint.Begin(menu.GetBounds());
        menu.Paint(paint);
        // The checkbox background samples a gradient at pixel centers, so its
        // endpoint color need not occur. Verify the actual check glyph instead.
        EXPECT_TRUE(HasColorInRect(paint.GetDrawData(), th.colors.textOnAccent, checkRect));
        EXPECT_TRUE(TextureHasColor(paint.GetDrawData(), icon, th.colors.text));
        checked = false;
        EXPECT_TRUE(menu.RefreshActionStates());
        paint.Begin(menu.GetBounds());
        menu.Paint(paint);
        EXPECT_FALSE(HasColorInRect(paint.GetDrawData(), th.colors.textOnAccent, checkRect));
        visible = false;
        EXPECT_TRUE(menu.RefreshActionStates());
        EXPECT_TRUE(menu.IsOpen());
        EXPECT_FLOAT_EQ(menu.PopupHeight(), twoRows - th.metrics.popupItemHeight);
        menu.OnPointerEvent(MenuRow(menu)); // hidden first row has no hit slot
        EXPECT_EQ(calls, 0);
        EXPECT_EQ(tailCalls, 1);
        OpenMenu(menu);
        registry->Remove("global", tail.id);
        menu.SetMenus(ActionBarBuilder::BuildMenus(invoke), true);
        EXPECT_FALSE(menu.IsOpen()); // all action rows are hidden
    }

    TEST_F(EditorActionStateTest, WidgetRefreshBoundsRecursionAndRejectsReplacedItemStorage)
    {
        ToolBar       toolbar(nullptr);
        int           toolbarResolves = 0;
        ToolBar::Item item;
        item.label        = "Recursive";
        item.resolveState = [&]() {
            ++toolbarResolves;
            EXPECT_FALSE(toolbar.RefreshActionStates());
            return EditorActionState{};
        };
        toolbar.SetItems({item});
        EXPECT_TRUE(toolbar.RefreshActionStates());
        EXPECT_EQ(toolbarResolves, 1);
        EXPECT_TRUE(toolbar.RefreshActionStates());
        EXPECT_EQ(toolbarResolves, 2);
        item.resolveState = [&]() {
            toolbar.SetItems({});
            return EditorActionState{};
        };
        toolbar.SetItems({item});
        EXPECT_FALSE(toolbar.RefreshActionStates());
        EXPECT_TRUE(toolbar.RefreshActionStates());

        MenuBar       menu({}, nullptr);
        int           menuResolves = 0;
        MenuBar::Item row;
        row.label        = "Recursive";
        row.resolveState = [&]() {
            ++menuResolves;
            EXPECT_FALSE(menu.RefreshActionStates());
            return EditorActionState{};
        };
        menu.SetMenus({{"Edit", {row}}});
        EXPECT_EQ(menuResolves, 1);
        EXPECT_TRUE(menu.RefreshActionStates());
        EXPECT_EQ(menuResolves, 2);
        row.resolveState = [&]() {
            menu.SetMenus({});
            return EditorActionState{};
        };
        menu.SetMenus({{"Edit", {row}}});
        EXPECT_TRUE(menu.RefreshActionStates());
        EXPECT_FALSE(menu.IsOpen());
    }

    TEST_F(EditorActionStateTest, PointerRefreshSurvivesDestructionOfEitherChromeOwner)
    {
        auto          toolbar = std::make_unique<ToolBar>(nullptr);
        ToolBar::Item item;
        item.label        = "Destroy";
        item.resolveState = [&]() {
            toolbar.reset();
            return EditorActionState{};
        };
        toolbar->SetItems({item});
        auto *oldToolbar = toolbar.get();
        EXPECT_EQ(oldToolbar->OnPointerEvent(Down(10, 10)), ui::UIEventResult::UNHANDLED);
        EXPECT_EQ(toolbar, nullptr);

        auto          menu  = std::make_unique<MenuBar>(std::vector<MenuBar::Menu>{}, nullptr);
        bool          armed = false;
        MenuBar::Item row;
        row.label        = "Destroy";
        row.resolveState = [&]() {
            if (armed)
                menu.reset();
            return EditorActionState{};
        };
        menu->SetMenus({{"Edit", {row}}});
        armed         = true;
        auto *oldMenu = menu.get();
        EXPECT_EQ(oldMenu->OnPointerEvent(Down(10, 10)), ui::UIEventResult::UNHANDLED);
        EXPECT_EQ(menu, nullptr);
    }

    TEST_F(EditorActionStateTest, ShellRefreshUpdatesBothChromeSurfacesWithoutClosingOpenMenu)
    {
        int  calls   = 0;
        bool enabled = false, checked = false;
        auto action      = Action(calls);
        action.enabled   = [&]() { return enabled; };
        action.checkable = true;
        action.checked   = [&]() { return checked; };
        EditorActionRegistry::Get()->Add(action);
        EditorShell shell;
        shell.SetIcon("action-state-icon", 233);
        shell.Rebuild();
        shell.Layout(800, 600);
        const auto &m       = GetDefaultUiTheme().metrics;
        auto       *menu    = dynamic_cast<MenuBar *>(shell.HitTest(m.padX + 1, m.menuBarHeight * 0.5f));
        auto       *toolbar = dynamic_cast<ToolBar *>(shell.HitTest(m.padX + 1, m.menuBarHeight + m.toolbarHeight * 0.5f));
        ASSERT_NE(menu, nullptr);
        ASSERT_NE(toolbar, nullptr);
        OpenMenu(*menu);
        enabled = true;
        checked = true;
        shell.Layout(800, 600);
        EXPECT_TRUE(menu->IsOpen());
        EXPECT_EQ(shell.HitTest(m.padX + 1, m.menuBarHeight * 0.5f), menu);
        int  addedCalls = 0;
        auto added      = Action(addedCalls);
        added.id        = "state.added";
        added.order     = 1;
        EditorActionRegistry::Get()->Add(added);
        shell.RefreshActions();
        EXPECT_TRUE(menu->IsOpen());
        ui::UIPaintContext paint;
        paint.Begin({0, 0, 800, 600});
        toolbar->Paint(paint);
        EXPECT_TRUE(HasColor(paint.GetDrawData(), GetDefaultUiTheme().colors.accentSoft));
        menu->OnPointerEvent(MenuRow(*menu));
        EXPECT_EQ(calls, 1);
    }

    TEST_F(EditorActionStateTest, ShellRefreshesLateAndReplacedToolbarIcons)
    {
        int calls = 0;
        EditorActionRegistry::Get()->Add(Action(calls));
        EditorShell shell;
        shell.Rebuild();
        shell.Layout(800, 600);
        ui::UIPaintContext paint;
        for (const auto texture : {233u, 234u, 234u}) {
            shell.SetIcon("action-state-icon", texture);
            paint.Begin({0, 0, 800, 600});
            shell.PaintSurface(0, paint, 800, 600);
            EXPECT_TRUE(HasTexture(paint.GetDrawData(), texture));
            EXPECT_FALSE(HasTexture(paint.GetDrawData(), texture == 233 ? 234 : 233));
        }
        shell.SetIcon("replacement-icon", 235);
        auto replacement = Action(calls);
        replacement.icon = "replacement-icon";
        EditorActionRegistry::Get()->Add(replacement);
        paint.Begin({0, 0, 800, 600});
        shell.PaintSurface(0, paint, 800, 600);
        EXPECT_TRUE(HasTexture(paint.GetDrawData(), 235));
        EXPECT_FALSE(HasTexture(paint.GetDrawData(), 234));
    }

    TEST_F(EditorActionStateTest, ShellRoutesTallSubmenuRowsAndOutsideDismissal)
    {
        int calls = 0;
        for (uint32_t row = 0; row < 6; ++row) {
            auto action    = Action(calls);
            action.id      = "submenu." + std::to_string(row);
            action.submenu = "C";
            action.order   = static_cast<int>(row);
            EditorActionRegistry::Get()->Add(action);
        }
        EditorShell shell;
        shell.Rebuild();
        shell.Layout(800, 600);
        const auto &m    = GetDefaultUiTheme().metrics;
        const float x    = m.padX + m.controlPad;
        auto       *menu = shell.HitTest(x, m.menuBarHeight * .5f);
        ASSERT_NE(menu, nullptr);
        ASSERT_STREQ(menu->GetTypeName(), "MenuBar");
        const auto openSubmenu = [&]() {
            shell.DispatchPointer(Down(x, m.menuBarHeight * .5f));
            auto hover   = Down(x, m.menuBarHeight + m.panelGap + m.popupItemHeight * .5f);
            hover.action = ui::UIPointerAction::MOVE;
            shell.DispatchPointer(hover);
            shell.Layout(800, 600);
        };
        openSubmenu();
        const auto lastRow = Down(m.padX + m.controlButtonWidth + m.controlPad, m.menuBarHeight + 2 * m.panelGap + m.popupItemHeight * 5.5f);
        ASSERT_EQ(shell.HitTest(lastRow.x, lastRow.y), menu);
        EXPECT_TRUE(shell.DispatchPointer(lastRow));
        EXPECT_EQ(calls, 1);
        shell.Layout(800, 600);
        EXPECT_NE(shell.HitTest(lastRow.x, lastRow.y), menu);
        openSubmenu();
        EXPECT_TRUE(shell.DispatchPointer(Down(700, 500)));
        shell.Layout(800, 600);
        EXPECT_EQ(calls, 1);
        EXPECT_NE(shell.HitTest(lastRow.x, lastRow.y), menu);
    }

    TEST_F(EditorActionStateTest, ViewerRefreshTracksHistoryStateWithoutRebuildingItsChrome)
    {
        int  calls   = 0;
        bool enabled = false, checked = false;
        auto action      = Action(calls);
        action.enabled   = [&]() { return enabled; };
        action.checkable = true;
        action.checked   = [&]() { return checked; };
        auto *registry   = EditorActionRegistry::Get();
        registry->Add(action);
        AssetViewerWidget viewer(nullptr);
        viewer.SetActionHandler([registry](const std::string &id) { return registry->Invoke(id); });
        viewer.Open(Uuid::Create());
        viewer.Arrange({0, 0, 1000, 700});
        auto *menu    = FindMenuBar(viewer);
        auto *toolbar = FindToolBar(viewer);
        ASSERT_NE(menu, nullptr);
        ASSERT_NE(toolbar, nullptr);
        OpenMenu(*menu);
        enabled = true;
        checked = true;
        viewer.SynchronizeLayout();
        EXPECT_EQ(menu, FindMenuBar(viewer));
        EXPECT_EQ(toolbar, FindToolBar(viewer));
        EXPECT_TRUE(menu->IsOpen());
        menu->OnPointerEvent(MenuRow(*menu));
        EXPECT_EQ(calls, 1);
    }

    TEST_F(EditorActionStateTest, ShellShortcutStateQueryMayDestroyTheShell)
    {
        int  calls     = 0;
        auto shell     = std::make_unique<EditorShell>();
        auto action    = Action(calls);
        action.enabled = [&]() {
            shell.reset();
            return true;
        };
        EditorActionRegistry::Get()->Add(action);
        auto *oldShell = shell.get();
        EXPECT_TRUE(oldShell->DispatchKey(Shortcut()));
        EXPECT_EQ(shell, nullptr);
        EXPECT_EQ(calls, 0);
    }

    TEST_F(EditorActionStateTest, ViewerCloseDuringInitialStateQueryCannotBeReopenedByTheOldOpen)
    {
        int               calls = 0, closed = 0;
        AssetViewerWidget viewer(nullptr);
        viewer.SetOnClosed([&]() { ++closed; });
        bool firstQuery = true;
        auto action     = Action(calls);
        action.visible  = [&]() {
            if (firstQuery) {
                firstQuery = false;
                viewer.Close();
            }
            return true;
        };
        EditorActionRegistry::Get()->Add(action);
        viewer.Open(Uuid::Create());
        EXPECT_FALSE(viewer.IsOpen());
        EXPECT_LE(closed, 1);
        EXPECT_EQ(calls, 0);
    }

    TEST_F(EditorActionStateTest, ShellMainPaintStopsWhenStateQueryDestroysItsOwner)
    {
        int  calls     = 0;
        bool armed     = false;
        auto shell     = std::make_unique<EditorShell>();
        auto action    = Action(calls);
        action.enabled = [&]() {
            if (armed)
                shell.reset();
            return true;
        };
        EditorActionRegistry::Get()->Add(action);
        shell->Rebuild();
        armed                       = true;
        auto              *oldShell = shell.get();
        ui::UIPaintContext paint;
        paint.Begin({0, 0, 800, 600});
        oldShell->PaintSurface(0, paint, 800, 600);
        EXPECT_EQ(shell, nullptr);
        EXPECT_EQ(calls, 0);
    }
} // namespace sky::editor::action_state_test
