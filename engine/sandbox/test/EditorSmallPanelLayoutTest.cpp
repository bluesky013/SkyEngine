#include "ReflectionTestEnvironment.h"
#include <core/type/TypeInfoObj.h>
#include <editor/core/EditorCore.h>
#include <editor/core/input/KeyModifiers.h>
#include <editor/shell/NewWorldDialog.h>
#include <editor/shell/WorldConfigPanel.h>
#include <editor/ui/UiTheme.h>
#include <framework/serialization/SerializationContext.h>
#include <framework/world/WorldSubSystemRegistry.h>
#include <gtest/gtest.h>
#include <ui/UIPaintContext.h>

#include <algorithm>
#include <cmath>
#include <filesystem>

namespace sky::editor::small_panel_layout_test {
    struct Config {
        float value = 1.0f;
    };

    namespace {
        void ExpectInside(const ui::UIRect &inner, const ui::UIRect &outer)
        {
            EXPECT_TRUE(std::isfinite(inner.left));
            EXPECT_TRUE(std::isfinite(inner.top));
            EXPECT_TRUE(std::isfinite(inner.right));
            EXPECT_TRUE(std::isfinite(inner.bottom));
            EXPECT_GE(inner.right, inner.left);
            EXPECT_GE(inner.bottom, inner.top);
            EXPECT_GE(inner.left + 0.01f, outer.left);
            EXPECT_GE(inner.top + 0.01f, outer.top);
            EXPECT_LE(inner.right - 0.01f, outer.right);
            EXPECT_LE(inner.bottom - 0.01f, outer.bottom);
        }
        void ExpectSame(const ui::UIRect &a, const ui::UIRect &b)
        {
            EXPECT_FLOAT_EQ(a.left, b.left);
            EXPECT_FLOAT_EQ(a.top, b.top);
            EXPECT_FLOAT_EQ(a.right, b.right);
            EXPECT_FLOAT_EQ(a.bottom, b.bottom);
        }
        ui::UIPointerEvent Down(const ui::UIRect &rect)
        {
            ui::UIPointerEvent event;
            event.action = ui::UIPointerAction::DOWN;
            event.x      = (rect.left + rect.right) * 0.5f;
            event.y      = (rect.top + rect.bottom) * 0.5f;
            return event;
        }
        ReflectedFormView *Form(ui::UIElement &element)
        {
            if (auto *form = dynamic_cast<ReflectedFormView *>(&element))
                return form;
            for (const auto &child : element.GetChildren()) {
                if (auto *form = Form(*child))
                    return form;
            }
            return nullptr;
        }
        PropertyField *Value(ReflectedFormView &form)
        {
            for (auto &section : form.Form().GetSections()) {
                for (auto &field : section.fields)
                    if (field.path == "value")
                        return &field;
            }
            return nullptr;
        }
        std::size_t TreeSize(const ui::UIElement &element)
        {
            std::size_t count = 1;
            for (const auto &child : element.GetChildren())
                count += TreeSize(*child);
            return count;
        }
        void Paint(ui::UIElement &element)
        {
            ui::UIPaintContext paint;
            paint.Begin(element.GetBounds());
            element.Paint(paint);
        }

        const auto *reflectionEnvironment = sky::editor::test::AddReflectionEnvironment(
            [] { SerializationContext::Get()->Register<Config>("EditorSmallPanelLayoutConfig").Member<&Config::value>("value"); });

        class EditorSmallPanelLayoutTest : public testing::Test {
        protected:
            void SetUp() override
            {
                saved = GetDefaultUiTheme();
                SetDefaultUiTheme(MakeDarkTheme());
                EditorCore::GetCommandService().Clear();
                WorldSubSystemRegistry::Get().Clear();
                WorldSubSystemRegistry::Get().Register(Name("SmallPanelConfig"),
                                                       {[](World &, const Any &) -> std::unique_ptr<IWorldSubSystem> { return nullptr; },
                                                        TypeInfoObj<Config>::Get()->RtInfo(),
                                                        [] { return Any(std::in_place_type<Config>); },
                                                        {}});
                path = std::filesystem::temp_directory_path() / "sky_small_panel_layout.world";
                std::error_code error;
                std::filesystem::remove(path, error);
            }
            void TearDown() override
            {
                EditorCore::GetCommandService().Clear();
                WorldSubSystemRegistry::Get().Clear();
                SetDefaultUiTheme(saved);
                std::error_code error;
                std::filesystem::remove(path, error);
            }
            UiTheme               saved;
            std::filesystem::path path;
        };
    } // namespace

    TEST_F(EditorSmallPanelLayoutTest, NewWorldQueriesShareAllocatedSlotsBeforePaintAcrossResizeAndThemeChanges)
    {
        NewWorldDialog dialog(nullptr);
        dialog.Open("/project/worlds", "Main");
        const ui::UIRect window{13.25f, 27.75f, 433.25f, 307.75f};
        dialog.ArrangeIfNeeded(window);
        ExpectInside(dialog.PanelRect(), window);
        ExpectInside(dialog.NameFieldRect(), dialog.PanelRect());
        ExpectInside(dialog.LocationFieldRect(), dialog.PanelRect());
        EXPECT_GE(dialog.LocationFieldRect().top, dialog.NameFieldRect().bottom);
        const auto originalField  = dialog.NameFieldRect();
        const auto originalButton = dialog.ButtonRect(0);
        for (int index = 0; index != 2; ++index) {
            const auto rect = dialog.ButtonRect(index);
            ExpectInside(rect, dialog.PanelRect());
            const auto event = Down(rect);
            EXPECT_EQ(dialog.ButtonAt(event.x, event.y), index);
            EXPECT_GE(rect.top, dialog.LocationFieldRect().bottom);
        }
        const auto count = TreeSize(dialog);
        Paint(dialog);
        EXPECT_EQ(TreeSize(dialog), count);
        ExpectSame(dialog.NameFieldRect(), originalField);
        UiTheme changed             = GetDefaultUiTheme();
        changed.metrics.frameHeight = 41.0f;
        changed.metrics.formLabelWidth += 14.0f;
        SetDefaultUiTheme(changed);
        dialog.SynchronizeLayout();
        EXPECT_GT(dialog.NameFieldRect().Height(), originalField.Height());
        dialog.ArrangeIfNeeded({0.0f, 0.0f, 900.0f, 640.0f});
        SetDefaultUiTheme(MakeDarkTheme(1.5f));
        dialog.SynchronizeLayout();
        EXPECT_GT(dialog.ButtonRect(0).Width(), originalButton.Width());
        ExpectInside(dialog.PanelRect(), dialog.GetBounds());
    }

    TEST_F(EditorSmallPanelLayoutTest, NewWorldNameLocationAndCreateUseTheNewSlotsWithoutPainting)
    {
        std::string    created;
        int            closes = 0;
        NewWorldDialog dialog(nullptr);
        dialog.SetOnCreate([&](const std::string &value) { created = value; });
        dialog.SetOnClosed([&]() { ++closes; });
        dialog.Open(path.parent_path().string(), "");
        dialog.ArrangeIfNeeded({0.0f, 0.0f, 700.0f, 500.0f});
        dialog.OnPointerEvent(Down(dialog.ButtonRect(0)));
        EXPECT_TRUE(dialog.IsOpen());
        EXPECT_TRUE(created.empty());
        dialog.OnPointerEvent(Down(dialog.NameFieldRect()));
        dialog.OnTextInput({"Scene"});
        dialog.OnPointerEvent(Down(dialog.ButtonRect(0)));
        EXPECT_FALSE(dialog.IsOpen());
        EXPECT_EQ(std::filesystem::path(created), path.parent_path() / "Scene.world");
        EXPECT_EQ(closes, 1);
        dialog.Open("/project", "Discard");
        dialog.OnPointerEvent(Down(dialog.ButtonRect(1)));
        EXPECT_FALSE(dialog.IsOpen());
        EXPECT_EQ(closes, 2);
    }

    TEST_F(EditorSmallPanelLayoutTest, NewWorldDegenerateAllocationRestoresValidTargetsWhenTheWindowGrows)
    {
        NewWorldDialog dialog(nullptr);
        dialog.Open("/project", "Main");
        for (const auto size : {ui::UISize{0.0f, 0.0f}, ui::UISize{1.0f, 1.0f}, ui::UISize{70.0f, 50.0f}}) {
            const ui::UIRect bounds{0.0f, 0.0f, size.width, size.height};
            dialog.ArrangeIfNeeded(bounds, size);
            ExpectInside(dialog.PanelRect(), bounds);
            ExpectInside(dialog.NameFieldRect(), dialog.PanelRect());
            ExpectInside(dialog.LocationFieldRect(), dialog.PanelRect());
            for (int index = 0; index != 2; ++index)
                ExpectInside(dialog.ButtonRect(index), dialog.PanelRect());
        }
        dialog.ArrangeIfNeeded({0.0f, 0.0f, 1000.0f, 760.0f});
        EXPECT_GT(dialog.NameFieldRect().Width(), 0.0f);
        EXPECT_GT(dialog.LocationFieldRect().Height(), 0.0f);
        for (int index = 0; index != 2; ++index) {
            const auto event = Down(dialog.ButtonRect(index));
            EXPECT_EQ(dialog.ButtonAt(event.x, event.y), index);
        }
    }

    TEST_F(EditorSmallPanelLayoutTest, WorldConfigAllocatesListAndFormBeforePaintAndTracksHeaderMetrics)
    {
        WorldConfigPanel panel(nullptr, {}, "Config");
        panel.SetDocument(nullptr);
        panel.ArrangeIfNeeded({5.25f, 7.75f, 125.25f, 107.75f});
        auto *form = Form(panel);
        ASSERT_NE(form, nullptr);
        EXPECT_FALSE(form->Form().IsValid());
        EXPECT_FALSE(form->IsVisible());
        ExpectInside(panel.ListRect(), panel.GetBounds());
        ExpectInside(form->GetBounds(), panel.GetBounds());
        EXPECT_FLOAT_EQ(form->GetBounds().left, panel.ListRect().right);
        panel.ArrangeIfNeeded({0.0f, 0.0f, 600.0f, 400.0f});
        UiTheme changed                   = GetDefaultUiTheme();
        changed.metrics.listColumnWidth   = 175.0f;
        changed.metrics.panelHeaderHeight = 39.0f;
        changed.metrics.rowHeight         = 38.0f;
        SetDefaultUiTheme(changed);
        panel.SynchronizeLayout();
        EXPECT_FLOAT_EQ(panel.ListRect().Width(), 175.0f);
        EXPECT_FLOAT_EQ(panel.ListRect().top, 39.0f);
        const auto count  = TreeSize(panel);
        const auto before = form->GetBounds();
        Paint(panel);
        EXPECT_EQ(TreeSize(panel), count);
        ExpectSame(form->GetBounds(), before);
        panel.SetTitleBarVisible(false);
        panel.SynchronizeLayout();
        EXPECT_FLOAT_EQ(panel.ListRect().top, panel.GetBounds().top);
    }

    TEST_F(EditorSmallPanelLayoutTest, WorldConfigReflectedEditsUndoAndSubsystemToggleStillUseTheDocument)
    {
        WorldDocument    document(path.string());
        WorldConfigPanel panel(nullptr, {}, "Config");
        panel.SetDocument(&document);
        panel.ArrangeIfNeeded({0.0f, 0.0f, 600.0f, 400.0f});
        auto *form = Form(panel);
        ASSERT_NE(form, nullptr);
        auto *field = Value(*form);
        ASSERT_NE(field, nullptr);
        ASSERT_TRUE(form->Form().Edit(*field, Any(2.0f), form->Commands()));
        EXPECT_TRUE(document.IsDirty());
        EXPECT_FLOAT_EQ(document.EnsureSubSystemConfig("SmallPanelConfig")->GetAsConst<Config>()->value, 2.0f);
        ASSERT_TRUE(form->Commands().Undo());
        EXPECT_FLOAT_EQ(document.EnsureSubSystemConfig("SmallPanelConfig")->GetAsConst<Config>()->value, 1.0f);
        panel.OnPointerEvent(Down(panel.CheckRect(0)));
        bool enabled = true;
        ASSERT_TRUE(document.IsSubSystemEnabled("SmallPanelConfig", enabled));
        EXPECT_FALSE(enabled);
    }

    TEST_F(EditorSmallPanelLayoutTest, WorldConfigDegenerateAndResizedRegionsDoNotOverlapOrEscapeThePanel)
    {
        WorldDocument    document(path.string());
        WorldConfigPanel panel(nullptr, {}, "Config");
        panel.SetDocument(&document);
        auto *form = Form(panel);
        ASSERT_NE(form, nullptr);
        for (const auto size : {ui::UISize{0.0f, 0.0f}, ui::UISize{1.0f, 1.0f}, ui::UISize{80.0f, 50.0f}, ui::UISize{700.0f, 420.0f}}) {
            panel.ArrangeIfNeeded({0.0f, 0.0f, size.width, size.height}, size);
            ExpectInside(panel.ListRect(), panel.GetBounds());
            ExpectInside(form->GetBounds(), panel.GetBounds());
            EXPECT_FLOAT_EQ(form->GetBounds().left, panel.ListRect().right);
        }
        EXPECT_GT(form->GetBounds().Width(), 0.0f);
        ExpectInside(panel.CheckRect(0), panel.RowRect(0));
    }

} // namespace sky::editor::small_panel_layout_test
