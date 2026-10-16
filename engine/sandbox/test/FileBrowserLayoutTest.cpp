#include <editor/shell/FileBrowserDialog.h>
#include <editor/ui/UiTheme.h>
#include <gtest/gtest.h>
#include <ui/UIPaintContext.h>
#include <ui/text/UIBuiltinFontProvider.h>
#include <ui/text/UITextSystem.h>
#include <ui/widgets/DockPanel.h>
#include <ui/widgets/GridPanel.h>
#include <ui/widgets/ScrollView.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

namespace sky::editor::file_browser_layout_test {
    namespace {
        class Source : public IFileBrowserSource {
        public:
            std::string Root() const override
            {
                return "/root";
            }
            std::string Parent(const std::string &) const override
            {
                return Root();
            }
            std::string Join(const std::string &location, const std::string &name) const override
            {
                return location + "/" + name;
            }
            bool IsDirectory(const std::string &location) const override
            {
                return location == Root() || location == "/root/sub" || location.find("/place/") == 0;
            }
            bool List(const std::string &location, std::vector<FileBrowserEntry> &out, std::string &error) const override
            {
                out.clear();
                error.clear();
                if (location == "/root/sub") {
                    out.push_back({"inside.txt", Join(location, "inside.txt"), false, {}});
                    return true;
                }
                out.push_back({"sub", Join(location, "sub"), true, {}});
                for (int i = 0; i < 100; ++i) {
                    const auto name = "entry" + std::to_string(i) + (i % 2 == 0 ? ".txt" : ".asset");
                    out.push_back({name, Join(location, name), false, {}});
                }
                return true;
            }
            std::vector<FileBrowserPlace> Places() const override
            {
                std::vector<FileBrowserPlace> result;
                for (int i = 0; i < 40; ++i)
                    result.push_back({std::string(70, 'p') + std::to_string(i), "/place/" + std::to_string(i)});
                return result;
            }
        };

        void Inside(const ui::UIRect &rect, const ui::UIRect &parent)
        {
            EXPECT_TRUE(std::isfinite(rect.left));
            EXPECT_TRUE(std::isfinite(rect.top));
            EXPECT_TRUE(std::isfinite(rect.right));
            EXPECT_TRUE(std::isfinite(rect.bottom));
            EXPECT_GE(rect.right, rect.left);
            EXPECT_GE(rect.bottom, rect.top);
            EXPECT_GE(rect.left + 0.01f, parent.left);
            EXPECT_GE(rect.top + 0.01f, parent.top);
            EXPECT_LE(rect.right - 0.01f, parent.right);
            EXPECT_LE(rect.bottom - 0.01f, parent.bottom);
        }

        void Same(const ui::UIRect &a, const ui::UIRect &b)
        {
            EXPECT_FLOAT_EQ(a.left, b.left);
            EXPECT_FLOAT_EQ(a.top, b.top);
            EXPECT_FLOAT_EQ(a.right, b.right);
            EXPECT_FLOAT_EQ(a.bottom, b.bottom);
        }

        const ui::UIElement *Named(const ui::UIElement &root, const std::string &name)
        {
            if (root.GetName() == name)
                return &root;
            for (const auto &child : root.GetChildren())
                if (const auto *found = Named(*child, name))
                    return found;
            return nullptr;
        }

        template <class T>
        int Count(const ui::UIElement &root)
        {
            int result = dynamic_cast<const T *>(&root) != nullptr ? 1 : 0;
            for (const auto &child : root.GetChildren())
                result += Count<T>(*child);
            return result;
        }

        ui::UIPointerEvent Pointer(ui::UIPointerAction action, const ui::UIRect &rect, float wheel = 0.0f)
        {
            ui::UIPointerEvent event;
            event.action     = action;
            event.x          = (rect.left + rect.right) * 0.5f;
            event.y          = (rect.top + rect.bottom) * 0.5f;
            event.wheelDelta = wheel;
            return event;
        }

        class FileBrowserLayoutTest : public testing::Test {
        protected:
            void SetUp() override
            {
                previousTheme = GetDefaultUiTheme();
                SetDefaultUiTheme(MakeDarkTheme());
                dialog = std::make_unique<FileBrowserDialog>(nullptr);
                dialog->Model().SetSource(&source);
                dialog->SetBounds({0.0f, 0.0f, 1100.0f, 760.0f});
                request.directory = source.Root();
                request.title     = std::string(140, 'T');
                request.filters   = {{"Text files with a very long filter label", {"txt"}}, {"Assets", {"asset"}}};
                dialog->Open(request);
            }
            void TearDown() override
            {
                dialog.reset();
                SetDefaultUiTheme(previousTheme);
            }
            void CheckRegions()
            {
                const auto panel = dialog->PanelRect();
                Inside(panel, dialog->GetBounds());
                Inside(dialog->SidebarRect(), panel);
                Inside(dialog->ToolbarRect(), panel);
                Inside(dialog->ListHeaderRect(), panel);
                Inside(dialog->ListRect(), panel);
                Inside(dialog->FooterRect(), panel);
                Inside(dialog->UpRect(), dialog->ToolbarRect());
                Inside(dialog->PathRect(), dialog->ToolbarRect());
                if (dialog->WriteMode())
                    Inside(dialog->NewFolderRect(), dialog->ToolbarRect());
                Inside(dialog->FilterRect(), dialog->FooterRect());
                Inside(dialog->NameFieldRect(), dialog->FooterRect());
                for (int i = 0; i < 2; ++i)
                    Inside(dialog->ButtonRect(i), dialog->FooterRect());
                EXPECT_LE(dialog->SidebarRect().right, dialog->ToolbarRect().left);
                EXPECT_LE(dialog->ToolbarRect().bottom, dialog->ListHeaderRect().top);
                EXPECT_LE(dialog->ListHeaderRect().bottom, dialog->ListRect().top);
                EXPECT_LE(dialog->ListRect().bottom, dialog->FooterRect().top);
            }
            void Click(const ui::UIRect &rect)
            {
                dialog->OnPointerEvent(Pointer(ui::UIPointerAction::DOWN, rect));
            }
            Source                             source;
            UiTheme                            previousTheme;
            FileBrowserRequest                 request;
            std::unique_ptr<FileBrowserDialog> dialog;
        };
    } // namespace

    TEST_F(FileBrowserLayoutTest, RegionsAndControlsAreArrangedContainerSlotsBeforeFirstPaint)
    {
        CheckRegions();
        EXPECT_GE(Count<ui::DockPanel>(*dialog), 5);
        EXPECT_EQ(Count<ui::GridPanel>(*dialog), 2);
        EXPECT_EQ(Count<ui::ScrollView>(*dialog), 2);
        const auto *name   = Named(*dialog, "file-browser-name");
        const auto *filter = Named(*dialog, "file-browser-filter");
        const auto *accept = Named(*dialog, "file-browser-accept");
        ASSERT_NE(name, nullptr);
        ASSERT_NE(filter, nullptr);
        ASSERT_NE(accept, nullptr);
        Same(dialog->NameFieldRect(), name->GetBounds());
        Same(dialog->FilterRect(), filter->GetBounds());
        Same(dialog->ButtonRect(0), accept->GetBounds());
        EXPECT_EQ(dialog->RowAt(dialog->NameFieldRect().left, dialog->NameFieldRect().top), -1);
    }

    TEST_F(FileBrowserLayoutTest, ResizeAndThemeChangesReflowWithoutPaintOrLosingModelSelection)
    {
        dialog->Model().SetSelected(1);
        dialog->Model().SetName("keep user name");
        for (const auto scale : {0.5f, 1.0f, 2.0f}) {
            SetDefaultUiTheme(MakeDarkTheme(scale));
            for (float width : {950.0f, 610.0f, 310.0f}) {
                dialog->SetBounds({15.0f, 25.0f, 15.0f + width, 705.0f});
                CheckRegions();
                EXPECT_EQ(dialog->Model().GetSelected(), 1);
                EXPECT_EQ(dialog->Model().GetName(), "keep user name");
                const auto cancel = dialog->ButtonRect(1);
                if (cancel.Width() > 0.0f && cancel.Height() > 0.0f)
                    EXPECT_EQ(dialog->ButtonAt((cancel.left + cancel.right) * 0.5f, (cancel.top + cancel.bottom) * 0.5f), 1);
            }
        }
    }

    TEST_F(FileBrowserLayoutTest, TinyViewportsClampAllRegionsAndPopupGeometry)
    {
        for (const auto size : {ui::UISize{280.0f, 230.0f}, ui::UISize{95.0f, 70.0f}, ui::UISize{12.0f, 9.0f}, ui::UISize{0.0f, 0.0f}}) {
            dialog->SetBounds({100.0f, 120.0f, 100.0f + size.width, 120.0f + size.height});
            CheckRegions();
            for (int i = 0; i < 3; ++i)
                Inside(dialog->FilterItemRect(i), dialog->GetBounds());
            EXPECT_EQ(dialog->RowAt(dialog->ButtonRect(0).left, dialog->ButtonRect(0).top), -1);
        }
    }

    TEST_F(FileBrowserLayoutTest, ListAndSidebarScrollIndependentlyAndUseArrangedRowsForSelection)
    {
        const auto initialPlace = dialog->PlaceRowRect(0);
        const auto initialRow   = dialog->RowRect(0);
        dialog->OnPointerEvent(Pointer(ui::UIPointerAction::WHEEL, dialog->ListRect(), -150.0f));
        EXPECT_LT(dialog->RowRect(0).top, initialRow.top);
        Same(dialog->PlaceRowRect(0), initialPlace);
        const auto scrolledRow = dialog->RowRect(0);
        dialog->OnPointerEvent(Pointer(ui::UIPointerAction::WHEEL, dialog->SidebarRect(), -150.0f));
        EXPECT_LT(dialog->PlaceRowRect(0).top, initialPlace.top);
        Same(dialog->RowRect(0), scrolledRow);
        const auto viewport = dialog->ListRect();
        int        target   = -1;
        for (int i = 1; i < static_cast<int>(dialog->Model().GetEntries().size()); ++i) {
            const auto row = dialog->RowRect(i);
            if (row.top >= viewport.top && row.bottom <= viewport.bottom) {
                target = i;
                break;
            }
        }
        ASSERT_GT(target, 0);
        const auto row = dialog->RowRect(target);
        EXPECT_EQ(dialog->RowAt((row.left + row.right) * 0.5f, (row.top + row.bottom) * 0.5f), target);
        Click(row);
        EXPECT_EQ(dialog->Model().GetSelected(), target);
        EXPECT_EQ(dialog->Model().GetName(), dialog->Model().GetEntries()[static_cast<std::size_t>(target)].name);
        const auto name = dialog->NameFieldRect();
        Click(name);
        EXPECT_EQ(dialog->Model().GetSelected(), target);
        EXPECT_EQ(dialog->RowAt((name.left + name.right) * 0.5f, (name.top + name.bottom) * 0.5f), -1);
    }

    TEST_F(FileBrowserLayoutTest, ScrolledRowsStayClippedAndPaintingConsumesPreparedSlots)
    {
        ui::UIBuiltinFontProvider font;
        ui::UITextSystem          text(&font, nullptr);
        dialog->SetTextSystem(&text);
        dialog->OnPointerEvent(Pointer(ui::UIPointerAction::WHEEL, dialog->ListRect(), -180.0f));
        const auto         name  = dialog->NameFieldRect();
        const auto         row   = dialog->RowRect(8);
        const auto         count = dialog->GetChildren().size();
        ui::UIPaintContext paint;
        paint.Begin(dialog->GetBounds());
        dialog->OnPaint(paint);
        Same(dialog->NameFieldRect(), name);
        Same(dialog->RowRect(8), row);
        EXPECT_EQ(dialog->GetChildren().size(), count);
        ASSERT_FALSE(paint.GetDrawData().vertices.empty());
        for (const auto &vertex : paint.GetDrawData().vertices) {
            EXPECT_TRUE(std::isfinite(vertex.x));
            EXPECT_TRUE(std::isfinite(vertex.y));
        }
    }

    TEST_F(FileBrowserLayoutTest, FilterPopupAndNavigationContinueUsingReflowedSlots)
    {
        dialog->SetBounds({0.0f, 0.0f, 600.0f, 560.0f});
        CheckRegions();
        EXPECT_EQ(dialog->Model().GetActiveFilter(), 0);
        const auto before = dialog->Model().GetEntries().size();
        Click(dialog->FilterRect());
        Click(dialog->FilterItemRect(0));
        EXPECT_EQ(dialog->Model().GetActiveFilter(), -1);
        EXPECT_GT(dialog->Model().GetEntries().size(), before);
        Click(dialog->RowRect(0));
        Click(dialog->RowRect(0));
        EXPECT_EQ(dialog->Model().GetLocation(), "/root/sub");
        Click(dialog->UpRect());
        EXPECT_EQ(dialog->Model().GetLocation(), "/root");
        EXPECT_GE(dialog->RowRect(0).top, dialog->ListRect().top);
    }

    TEST_F(FileBrowserLayoutTest, ReflowedFooterRoutesAcceptAndCancelWithoutSelectingListRows)
    {
        FileBrowserResult result;
        int               calls = 0;
        dialog->SetOnResult([&](const auto &value) {
            result = value;
            ++calls;
        });
        dialog->SetBounds({0.0f, 0.0f, 600.0f, 590.0f});
        Click(dialog->RowRect(1));
        const auto expected = dialog->Model().GetSelectedEntry()->path;
        Click(dialog->ButtonRect(0));
        EXPECT_FALSE(dialog->IsOpen());
        EXPECT_TRUE(result.accepted);
        EXPECT_EQ(result.path, expected);
        EXPECT_EQ(calls, 1);
        dialog->Open(request);
        dialog->SetBounds({0.0f, 0.0f, 300.0f, 590.0f});
        Click(dialog->NameFieldRect());
        EXPECT_EQ(dialog->Model().GetSelected(), -1);
        Click(dialog->ButtonRect(1));
        EXPECT_FALSE(dialog->IsOpen());
        EXPECT_FALSE(result.accepted);
        EXPECT_EQ(calls, 2);
    }

    TEST_F(FileBrowserLayoutTest, DirectoryModeKeepsToolbarControlsWithinTheAllocatedToolbar)
    {
        request.mode = FileBrowserMode::SELECT_DIRECTORY;
        dialog->Open(request);
        for (float width : {1050.0f, 450.0f, 140.0f}) {
            dialog->SetBounds({0.0f, 0.0f, width, 650.0f});
            CheckRegions();
            EXPECT_LE(dialog->UpRect().right, dialog->PathRect().left);
            EXPECT_LE(dialog->PathRect().right, dialog->NewFolderRect().left);
        }
    }
} // namespace sky::editor::file_browser_layout_test
