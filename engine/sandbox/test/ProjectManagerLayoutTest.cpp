#include <editor/shell/ProjectManagerView.h>
#include <editor/ui/UiTheme.h>
#include <gtest/gtest.h>
#include <ui/UIContext.h>
#include <ui/UIEventRouter.h>
#include <ui/UIPaintContext.h>
#include <ui/widgets/DockPanel.h>

#include <array>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

namespace sky::editor::project_manager_layout_test {

    namespace {
        bool HasArea(const ui::UIRect &rect)
        {
            return rect.right > rect.left && rect.bottom > rect.top;
        }

        void ExpectInside(const ui::UIRect &rect, const ui::UIRect &outer)
        {
            EXPECT_TRUE(std::isfinite(rect.left));
            EXPECT_TRUE(std::isfinite(rect.top));
            EXPECT_TRUE(std::isfinite(rect.right));
            EXPECT_TRUE(std::isfinite(rect.bottom));
            EXPECT_GE(rect.right, rect.left);
            EXPECT_GE(rect.bottom, rect.top);
            EXPECT_GE(rect.left + 0.01f, outer.left);
            EXPECT_GE(rect.top + 0.01f, outer.top);
            EXPECT_LE(rect.right - 0.01f, outer.right);
            EXPECT_LE(rect.bottom - 0.01f, outer.bottom);
        }

        void ExpectSeparate(const ui::UIRect &a, const ui::UIRect &b)
        {
            EXPECT_TRUE(!HasArea(a) || !HasArea(b) || a.right <= b.left || b.right <= a.left || a.bottom <= b.top || b.bottom <= a.top);
        }

        ui::UIPointerEvent Down(const ui::UIRect &rect)
        {
            ui::UIPointerEvent event;
            event.action = ui::UIPointerAction::DOWN;
            event.x      = (rect.left + rect.right) * 0.5f;
            event.y      = (rect.top + rect.bottom) * 0.5f;
            return event;
        }

        const ui::UIElement *FindBounds(const ui::UIElement &element, const ui::UIRect &rect)
        {
            const auto &bounds = element.GetBounds();
            if (bounds.left == rect.left && bounds.top == rect.top && bounds.right == rect.right && bounds.bottom == rect.bottom &&
                element.GetChildren().empty()) {
                return &element;
            }
            for (const auto &child : element.GetChildren()) {
                if (const auto *found = FindBounds(*child, rect)) {
                    return found;
                }
            }
            return nullptr;
        }

        struct Snapshot {
            const ui::UIElement *element;
            ui::UIRect           bounds;
        };

        void Capture(const ui::UIElement &element, std::vector<Snapshot> &out)
        {
            out.push_back({&element, element.GetBounds()});
            for (const auto &child : element.GetChildren()) {
                Capture(*child, out);
            }
        }

        class ProjectManagerLayoutTest : public testing::Test {
        protected:
            void SetUp() override
            {
                saved = GetDefaultUiTheme();
                SetDefaultUiTheme(MakeDarkTheme());
            }
            void TearDown() override
            {
                SetDefaultUiTheme(saved);
            }
            UiTheme saved;
        };
    } // namespace

    TEST_F(ProjectManagerLayoutTest, SharedSlotsPrepareControlsAndRecentViewportBeforePaint)
    {
        ProjectManagerView view;
        view.SetRecent({"/projects/first.skyproj", "/projects/second.skyproj"});
        view.Arrange({13.25f, 20.5f, 1013.25f, 720.5f}, {1000.0f, 700.0f});
        ASSERT_EQ(view.GetChildren().size(), 1u);
        EXPECT_NE(dynamic_cast<const ui::DockPanel *>(view.GetChildren().front().get()), nullptr);
        const auto banner = view.BannerRect();
        const auto list   = view.ListViewportRect();
        const auto status = view.StatusRect();
        ExpectInside(banner, view.GetBounds());
        ExpectInside(list, view.GetBounds());
        ExpectInside(status, view.GetBounds());
        EXPECT_GE(list.top, banner.bottom);
        EXPECT_LE(list.bottom, status.top);
        for (int index = 0; index != 3; ++index) {
            const auto top    = view.TopButtonRect(index);
            const auto action = view.ActionRect(index);
            EXPECT_TRUE(HasArea(top));
            EXPECT_TRUE(HasArea(action));
            ExpectInside(top, banner);
            ExpectInside(action, view.GetBounds());
            EXPECT_NE(FindBounds(view, top), nullptr);
            EXPECT_NE(FindBounds(view, action), nullptr);
            EXPECT_EQ(view.TopButtonAt(Down(top).x, Down(top).y), index);
            EXPECT_EQ(view.ActionAt(Down(action).x, Down(action).y), index);
        }
        EXPECT_EQ(view.RowAt(Down(view.RowRect(1)).x, Down(view.RowRect(1)).y), 1);
    }

    TEST_F(ProjectManagerLayoutTest, SmallAndDegenerateWindowsKeepControlTargetsBoundedAndSeparate)
    {
        ProjectManagerView view;
        view.SetRecent({"one.skyproj", "two.skyproj"});
        for (const auto size : {ui::UISize{360.0f, 500.0f}, ui::UISize{190.0f, 260.0f}, ui::UISize{24.0f, 18.0f}, ui::UISize{0.0f, 0.0f}}) {
            view.Arrange({0.0f, 0.0f, size.width, size.height}, size);
            std::array<ui::UIRect, 6> controls;
            for (int index = 0; index != 3; ++index) {
                controls[index]     = view.TopButtonRect(index);
                controls[index + 3] = view.ActionRect(index);
            }
            for (std::size_t index = 0; index != controls.size(); ++index) {
                ExpectInside(controls[index], view.GetBounds());
                for (std::size_t other = index + 1; other != controls.size(); ++other) {
                    ExpectSeparate(controls[index], controls[other]);
                }
            }
            ExpectInside(view.ListViewportRect(), view.GetBounds());
            EXPECT_EQ(view.TopButtonAt(-1.0f, -1.0f), -1);
            EXPECT_EQ(view.ActionAt(size.width + 1.0f, size.height + 1.0f), -1);
        }
        EXPECT_FALSE(HasArea(view.TopButtonRect(-1)));
        EXPECT_FALSE(HasArea(view.ActionRect(3)));
        EXPECT_FALSE(HasArea(view.RowRect(20)));
        EXPECT_EQ(view.RowAt(0.0f, 0.0f), -1);
    }

    TEST_F(ProjectManagerLayoutTest, ThemeAndResizeQueriesPrepareNewSlotsBeforePaint)
    {
        ProjectManagerView view;
        view.SetRecent({"one.skyproj"});
        view.Arrange({0.0f, 0.0f, 1100.0f, 800.0f}, {1100.0f, 800.0f});
        const auto first           = view.TopButtonRect(0);
        UiTheme    theme           = GetDefaultUiTheme();
        theme.metrics.frameHeight  = 41.0f;
        theme.metrics.bannerHeight = 100.0f;
        SetDefaultUiTheme(theme);
        const auto themed = view.TopButtonRect(0);
        EXPECT_GT(themed.Height(), first.Height());
        EXPECT_GE(view.BannerRect().Height(), 100.0f);
        SetDefaultUiTheme(MakeDarkTheme(1.5f));
        view.SetBounds({0.0f, 0.0f, 420.0f, 650.0f});
        const auto compact = view.TopButtonRect(0);
        ExpectInside(compact, view.BannerRect());
        EXPECT_NE(view.TopButtonRect(2).top, compact.top);
        EXPECT_EQ(view.TopButtonAt(Down(compact).x, Down(compact).y), 0);
    }

    TEST_F(ProjectManagerLayoutTest, ClippedRowsCannotSelectThroughStatusAndBusinessActionsKeepTheirPaths)
    {
        ProjectManagerView       view;
        std::vector<std::string> paths;
        for (int index = 0; index != 40; ++index) {
            paths.push_back("/projects/" + std::to_string(index) + ".skyproj");
        }
        view.SetRecent(paths);
        view.Arrange({0.0f, 0.0f, 1000.0f, 650.0f}, {1000.0f, 650.0f});
        const auto status = view.StatusRect();
        EXPECT_EQ(view.RowAt(Down(status).x, Down(status).y), -1);
        const auto clipped = view.RowRect(39);
        EXPECT_EQ(view.RowAt(Down(clipped).x, Down(clipped).y), -1);
        int opened  = 0;
        view.onOpen = [&](const std::string &path) {
            EXPECT_EQ(path, paths[0]);
            ++opened;
        };
        view.OnPointerEvent(Down(view.ActionRect(0))); // no selected project
        EXPECT_EQ(opened, 0);
        view.OnPointerEvent(Down(view.RowRect(0)));
        view.OnPointerEvent(Down(view.ActionRect(0)));
        EXPECT_EQ(opened, 1);
        std::string removed;
        view.onRemove = [&](const std::string &path) {
            view.SetRecent({"replacement.skyproj"});
            removed = path; // callback may replace the underlying recent vector
        };
        view.OnPointerEvent(Down(view.ActionRect(1)));
        EXPECT_EQ(removed, paths[0]);
        int deleted   = 0;
        view.onDelete = [&](const std::string &path) {
            EXPECT_EQ(path, "replacement.skyproj");
            ++deleted;
        };
        view.OnPointerEvent(Down(view.RowRect(0)));
        view.OnPointerEvent(Down(view.ActionRect(2)));
        EXPECT_EQ(deleted, 0);
        view.OnPointerEvent(Down(view.ActionRect(2)));
        EXPECT_EQ(deleted, 1);
    }

    TEST_F(ProjectManagerLayoutTest, PaintConsumesPreparedTreeAndLargeOriginKeepsLogicalSize)
    {
        ProjectManagerView view;
        view.SetRecent({std::string(150, 'a') + ".skyproj"});
        const ui::UISize logical{845.25f, 699.5f};
        view.Arrange({100000000.0f, 100000000.0f, 100000848.0f, 100000704.0f}, logical);
        ASSERT_EQ(view.GetChildren().size(), 1u);
        EXPECT_FLOAT_EQ(view.GetChildren().front()->GetArrangedSize().width, logical.width);
        EXPECT_FLOAT_EQ(view.GetChildren().front()->GetArrangedSize().height, logical.height);
        std::vector<Snapshot> before;
        Capture(view, before);
        ui::UIPaintContext paint;
        paint.Begin(view.GetBounds());
        view.OnPaint(paint);
        std::vector<Snapshot> after;
        Capture(view, after);
        ASSERT_EQ(after.size(), before.size());
        for (std::size_t index = 0; index != before.size(); ++index) {
            EXPECT_EQ(after[index].element, before[index].element);
            EXPECT_FLOAT_EQ(after[index].bounds.left, before[index].bounds.left);
            EXPECT_FLOAT_EQ(after[index].bounds.top, before[index].bounds.top);
            EXPECT_FLOAT_EQ(after[index].bounds.right, before[index].bounds.right);
            EXPECT_FLOAT_EQ(after[index].bounds.bottom, before[index].bounds.bottom);
        }
    }

    TEST_F(ProjectManagerLayoutTest, RouterBubblesFromControlSlotAndActionCanDestroyItsHost)
    {
        ui::UIContext     context;
        ui::UIEventRouter router(context);
        context.SetContentSize(900.0f, 600.0f);
        auto              *view = static_cast<ProjectManagerView *>(context.AddChild(std::make_unique<ProjectManagerView>()));
        ui::UILayoutParams fill;
        fill.anchorMaxX = fill.anchorMaxY = 1.0f;
        view->SetLayout(fill); // Match the host's whole-window hub allocation.
        int calls   = 0;
        view->onAdd = [&] {
            ++calls;
            context.GetRoot()->ClearChildren();
        };
        context.Layout();
        const auto target = view->TopButtonRect(0);
        ASSERT_TRUE(HasArea(target));
        const auto *leaf = FindBounds(*view, target);
        ASSERT_NE(leaf, nullptr);
        const auto event = Down(target);
        EXPECT_EQ(router.HitTest(event.x, event.y), leaf);
        EXPECT_EQ(router.DispatchPointer(event), ui::UIEventResult::HANDLED);
        EXPECT_EQ(calls, 1);
        EXPECT_TRUE(context.GetRoot()->GetChildren().empty());
        EXPECT_EQ(router.GetCapture(0), nullptr);
    }

} // namespace sky::editor::project_manager_layout_test
