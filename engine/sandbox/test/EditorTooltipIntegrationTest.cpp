//
// Created on 2026/11/15.
//

#include <editor/core/layout/LayoutModel.h>
#include <editor/core/layout/PanelRegistry.h>
#include <editor/shell/EditorShell.h>
#include <editor/widgets/MenuBar.h>
#include <editor/widgets/TooltipHost.h>
#include <gtest/gtest.h>
#include <ui/UIElement.h>
#include <ui/UIPaintContext.h>
#include <ui/UITooltip.h>

namespace sky::editor {
    namespace {
        class TooltipProbe final : public ui::UIElement {
        public:
            bool QueryTooltip(float x, float y, ui::UITooltipTarget &out) const override
            {
                if (!GetBounds().Contains(x, y))
                    return false;
                out = {1, GetBounds(), GetInputReference().lifetime};
                return true;
            }

            bool BuildTooltip(uint64_t id, std::string &out) const override
            {
                if (id != 1)
                    return false;
                ++builds;
                out = "Panel hint";
                return true;
            }

            mutable int builds = 0;
        };

        struct TooltipShell {
            TooltipShell()
            {
                layout.SetDefault({"a", "b"});
                registry.Register(PanelInfo{"a", "A", 0.0f, 0.0f, nullptr});
                registry.Register(PanelInfo{"b", "B", 0.0f, 0.0f, nullptr});
                shell.SetLayout(&layout);
                shell.SetPanelRegistry(&registry);
                shell.RegisterPanelView("a", [this]() {
                    auto view = std::make_unique<TooltipProbe>();
                    first     = view.get();
                    return view;
                });
                shell.RegisterPanelView("b", [this]() {
                    auto view = std::make_unique<TooltipProbe>();
                    second    = view.get();
                    return view;
                });
                shell.Rebuild();
                shell.Layout(1000.0f, 700.0f);
            }

            static ui::UIPointerEvent Hover(const ui::UIElement &element)
            {
                const auto &rect = element.GetBounds();
                return {.action = ui::UIPointerAction::MOVE, .x = (rect.left + rect.right) * 0.5f, .y = (rect.top + rect.bottom) * 0.5f};
            }

            LayoutModel   layout;
            PanelRegistry registry;
            EditorShell   shell;
            TooltipProbe *first  = nullptr;
            TooltipProbe *second = nullptr;
        };
    } // namespace

    TEST(EditorTooltipIntegrationTest, DelaysContentAndCancelsOnInputResetAndKeyboard)
    {
        TooltipShell test;
        ASSERT_NE(test.first, nullptr);
        test.shell.DispatchPointer(TooltipShell::Hover(*test.first));
        test.shell.TickTooltips(0.0f);
        test.shell.TickTooltips(0.49f);
        EXPECT_EQ(test.first->builds, 0);
        test.shell.TickTooltips(0.02f);
        ASSERT_EQ(test.first->builds, 1);
        test.shell.ResetInput();
        test.shell.TickTooltips(1.0f);
        EXPECT_EQ(test.first->builds, 1);

        test.shell.DispatchPointer(TooltipShell::Hover(*test.first));
        test.shell.TickTooltips(0.0f);
        test.shell.DispatchKey({.keyCode = 'A', .action = ui::UIKeyAction::DOWN});
        test.shell.TickTooltips(1.0f);
        EXPECT_EQ(test.first->builds, 1);
    }

    TEST(EditorTooltipIntegrationTest, ModalScopeBlocksHintsFromUnderlyingPanels)
    {
        TooltipShell test;
        ASSERT_NE(test.first, nullptr);
        const auto hover = TooltipShell::Hover(*test.first);
        test.shell.DispatchPointer(hover);
        test.shell.TickTooltips(0.0f);
        test.shell.OpenNewWorldDialog("", "", {});
        test.shell.Layout(1000.0f, 700.0f);
        test.shell.DispatchPointer(hover);
        test.shell.TickTooltips(1.0f);
        EXPECT_EQ(test.first->builds, 0);
    }

    TEST(EditorTooltipIntegrationTest, FloatingSurfaceOwnsItsDelayAndLosesItOnWindowSwitch)
    {
        TooltipShell test;
        ASSERT_EQ(test.shell.FloatPanelToSurface("b", 7, FloatingPanel{}), 7u);
        ASSERT_NE(test.second, nullptr);
        ui::UIPaintContext paint;
        test.shell.PaintSurface(7, paint, 600.0f, 400.0f);
        test.shell.Layout(1000.0f, 700.0f);

        test.shell.DispatchPointer(TooltipShell::Hover(*test.first));
        test.shell.TickTooltips(0.0f);
        test.shell.TickTooltips(0.3f);
        test.shell.DispatchPointerToSurface(7, TooltipShell::Hover(*test.second));
        test.shell.TickTooltips(0.0f);
        test.shell.TickTooltips(0.3f);
        EXPECT_EQ(test.first->builds, 0);
        EXPECT_EQ(test.second->builds, 0);
        test.shell.TickTooltips(0.21f);
        EXPECT_EQ(test.second->builds, 1);
        test.shell.ResetInput();
        test.shell.TickTooltips(1.0f);
        EXPECT_EQ(test.second->builds, 1);
    }

    TEST(EditorTooltipIntegrationTest, OpenMenuBlocksUnderlyingHintsInManuallyRoutedTrees)
    {
        ui::UIElement root;
        root.SetBounds({0.0f, 0.0f, 600.0f, 400.0f});
        auto  content = std::make_unique<TooltipProbe>();
        auto *probe   = content.get();
        content->SetBounds({0.0f, 30.0f, 600.0f, 400.0f});
        root.AddChild(std::move(content));
        MenuBar::Menu file;
        file.label = "File";
        file.items.push_back({"Open", []() {}});
        auto  menu = std::make_unique<MenuBar>(std::vector<MenuBar::Menu>{file}, nullptr);
        auto *bar  = menu.get();
        menu->SetBounds({0.0f, 0.0f, 600.0f, 30.0f});
        root.AddChild(std::move(menu));
        bar->OnPointerEvent({.action = ui::UIPointerAction::DOWN, .x = 15.0f, .y = 15.0f});
        ASSERT_TRUE(bar->IsOpen());
        ASSERT_TRUE(bar->WantsPointerCapture());
        TooltipHost host;
        host.Pointer({.action = ui::UIPointerAction::MOVE, .x = 50.0f, .y = 50.0f});
        host.Tick(&root, 1.0f);
        EXPECT_FALSE(host.IsVisible());
        EXPECT_EQ(probe->builds, 0);
    }
} // namespace sky::editor
