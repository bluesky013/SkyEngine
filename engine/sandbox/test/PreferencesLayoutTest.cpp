#include <core/environment/Once.h>
#include <editor/core/EditorCore.h>
#include <editor/core/input/KeyModifiers.h>
#include <editor/shell/PreferencesDialog.h>
#include <editor/ui/UiTheme.h>
#include <editor/widgets/GenericReflectedWidget.h>
#include <editor/widgets/ReflectedFormView.h>
#include <framework/serialization/SerializationContext.h>
#include <gtest/gtest.h>
#include <ui/UIPaintContext.h>
#include <ui/text/UIBuiltinFontProvider.h>
#include <ui/text/UITextSystem.h>
#include <ui/widgets/DockPanel.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace sky::editor::preferences_layout_test {

    struct PageData {
        int32_t value    = 1;
        bool    first    = false;
        bool    option01 = false;
        bool    option02 = false;
        bool    option03 = false;
        bool    option04 = false;
        bool    option05 = false;
        bool    option06 = false;
        bool    option07 = false;
        bool    option08 = false;
        bool    option09 = false;
        bool    option10 = false;
        bool    option11 = false;
        bool    last     = false;
    };

    namespace {

        ui::UIRect Intersect(const ui::UIRect &a, const ui::UIRect &b)
        {
            return {std::max(a.left, b.left), std::max(a.top, b.top), std::min(a.right, b.right), std::min(a.bottom, b.bottom)};
        }

        bool HasArea(const ui::UIRect &rect)
        {
            return rect.right > rect.left && rect.bottom > rect.top;
        }

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

        void ExpectSameRect(const ui::UIRect &actual, const ui::UIRect &expected)
        {
            EXPECT_FLOAT_EQ(actual.left, expected.left);
            EXPECT_FLOAT_EQ(actual.top, expected.top);
            EXPECT_FLOAT_EQ(actual.right, expected.right);
            EXPECT_FLOAT_EQ(actual.bottom, expected.bottom);
        }

        template <class T>
        T *Find(ui::UIElement &root)
        {
            if (auto *found = dynamic_cast<T *>(&root)) {
                return found;
            }
            for (const auto &child : root.GetChildren()) {
                if (auto *found = Find<T>(*child)) {
                    return found;
                }
            }
            return nullptr;
        }

        struct LayoutSnapshot {
            const ui::UIElement *element;
            ui::UIRect           bounds;
        };

        void Snapshot(const ui::UIElement &root, std::vector<LayoutSnapshot> &result)
        {
            result.push_back({&root, root.GetBounds()});
            for (const auto &child : root.GetChildren()) {
                Snapshot(*child, result);
            }
        }

        std::vector<LayoutSnapshot> Snapshot(const ui::UIElement &root)
        {
            std::vector<LayoutSnapshot> result;
            Snapshot(root, result);
            return result;
        }

        void ExpectSameTree(const ui::UIElement &root, const std::vector<LayoutSnapshot> &expected)
        {
            const auto current = Snapshot(root);
            ASSERT_EQ(current.size(), expected.size());
            for (std::size_t index = 0; index != current.size(); ++index) {
                EXPECT_EQ(current[index].element, expected[index].element);
                ExpectSameRect(current[index].bounds, expected[index].bounds);
            }
        }

        const ui::UIElement *FindLeafAtBounds(const ui::UIElement &root, const ui::UIRect &bounds)
        {
            const auto &actual = root.GetBounds();
            if (root.GetChildren().empty() && actual.left == bounds.left && actual.top == bounds.top && actual.right == bounds.right &&
                actual.bottom == bounds.bottom) {
                return &root;
            }
            for (const auto &child : root.GetChildren()) {
                if (const auto *found = FindLeafAtBounds(*child, bounds)) {
                    return found;
                }
            }
            return nullptr;
        }

        PropertyField *Field(ReflectedFormView &form, const std::string &path)
        {
            for (auto &section : form.Form().GetSections()) {
                for (auto &field : section.fields) {
                    if (field.path == path) {
                        return &field;
                    }
                }
            }
            return nullptr;
        }

        class RecordingBoolWidget : public GenericReflectedWidget {
        public:
            void Paint(ReflectedWidgetHost &host, ui::UIPaintContext &context, PropertyField &field, const ui::UIRect &rect) override
            {
                controls[field.path] = rect;
                GenericReflectedWidget::Paint(host, context, field, rect);
            }

            std::unordered_map<std::string, ui::UIRect> controls;
        };

        class DragProbeWidget : public RecordingBoolWidget {
        public:
            bool IsInteracting() const override
            {
                return dragging;
            }
            bool OnDown(ReflectedWidgetHost &, PropertyField &, const ui::UIPointerEvent &, const ui::UIRect &) override
            {
                dragging = true;
                ++downs;
                return true;
            }
            bool OnMove(ReflectedWidgetHost &, const ui::UIPointerEvent &) override
            {
                if (dragging) {
                    ++moves;
                }
                return dragging;
            }
            bool OnUp(ReflectedWidgetHost &, const ui::UIPointerEvent &) override
            {
                if (!dragging) {
                    return false;
                }
                dragging = false;
                ++ups;
                return true;
            }
            void ResetInteraction(ReflectedWidgetHost &, ReflectedInteractionReset) override
            {
                dragging = false;
                ++resets;
            }

            bool dragging = false;
            int  downs    = 0;
            int  moves    = 0;
            int  ups      = 0;
            int  resets   = 0;
        };

        class PopupProbeWidget : public RecordingBoolWidget {
        public:
            static constexpr uint32_t PopupColor = 0xFF12AB34;

            bool IsInteracting() const override
            {
                return open;
            }
            bool HasPopup() const override
            {
                return open;
            }
            bool OnDown(ReflectedWidgetHost &, PropertyField &, const ui::UIPointerEvent &, const ui::UIRect &) override
            {
                open = true;
                return true;
            }
            void PaintPopup(ReflectedWidgetHost &host, ui::UIPaintContext &context) override
            {
                hostBounds = host.ViewBounds();
                popup      = {hostBounds.left + 8.0f, hostBounds.top + 8.0f, hostBounds.right - 8.0f, hostBounds.bottom - 8.0f};
                context.AddRect(popup, PopupColor);
            }
            bool OnPopupPointer(ReflectedWidgetHost &, const ui::UIPointerEvent &event) override
            {
                if (!open || !popup.Contains(event.x, event.y)) {
                    return false;
                }
                if (event.action == ui::UIPointerAction::DOWN) {
                    ++downs;
                } else if (event.action == ui::UIPointerAction::UP) {
                    ++ups;
                }
                return true;
            }
            bool OnEscape(ReflectedWidgetHost &) override
            {
                open = false;
                return true;
            }
            void ResetInteraction(ReflectedWidgetHost &, ReflectedInteractionReset) override
            {
                open = false;
            }

            ui::UIRect hostBounds;
            ui::UIRect popup;
            bool       open  = false;
            int        downs = 0;
            int        ups   = 0;
        };

        class PreferencesLayoutTest : public testing::Test {
        protected:
            void SetUp() override
            {
                savedTheme = GetDefaultUiTheme();
                SetDefaultUiTheme(MakeDarkTheme());
                if (Once::Get()->Run("PreferencesLayoutTest.PageData")) {
                    SerializationContext::Get()
                        ->Register<PageData>("PreferencesLayoutPageData")
                        .Member<&PageData::value>("value")
                        .Member<&PageData::first>("first")
                        .Member<&PageData::option01>("option01")
                        .Member<&PageData::option02>("option02")
                        .Member<&PageData::option03>("option03")
                        .Member<&PageData::option04>("option04")
                        .Member<&PageData::option05>("option05")
                        .Member<&PageData::option06>("option06")
                        .Member<&PageData::option07>("option07")
                        .Member<&PageData::option08>("option08")
                        .Member<&PageData::option09>("option09")
                        .Member<&PageData::option10>("option10")
                        .Member<&PageData::option11>("option11")
                        .Member<&PageData::last>("last");
                }
                // This fixture owns its appearance overrides. ReflectedForm's
                // editor-side ordering follows the registry, independently of
                // serialization member-map iteration order.
                int order = 0;
                for (const char *name : {"value", "first", "option01", "option02", "option03", "option04", "option05", "option06", "option07",
                                         "option08", "option09", "option10", "option11", "last"}) {
                    EditorCore::GetPropertyEditors().RegisterMemberAppearance(TypeInfo<PageData>::RegisteredId(), name, {"", order++, true});
                }
                pages = PreferencePageRegistry::Get();
                pages->Clear();
                EditorCore::GetCommandService().Clear();
            }

            void TearDown() override
            {
                pages->Clear();
                EditorCore::GetCommandService().Clear();
                SetDefaultUiTheme(savedTheme);
            }

            void Add(const std::string &id, PageData &data, const std::string &title = {})
            {
                pages->Add({id, title.empty() ? id : title, &data, GetTypeNode(TypeInfo<PageData>::RegisteredId())});
            }

            static ui::UIPointerEvent Pointer(ui::UIPointerAction action, const ui::UIRect &rect, float wheel = 0.0f)
            {
                ui::UIPointerEvent event;
                event.action     = action;
                event.x          = (rect.left + rect.right) * 0.5f;
                event.y          = (rect.top + rect.bottom) * 0.5f;
                event.wheelDelta = wheel;
                return event;
            }

            static void Click(PreferencesDialog &dialog, const ui::UIRect &rect)
            {
                dialog.OnPointerEvent(Pointer(ui::UIPointerAction::DOWN, rect));
                dialog.OnPointerEvent(Pointer(ui::UIPointerAction::UP, rect));
            }

            static void Paint(PreferencesDialog &dialog)
            {
                ui::UIPaintContext context;
                context.Begin(dialog.GetBounds());
                dialog.Paint(context);
            }

            static int32_t WorkingValue(ReflectedFormView &form)
            {
                const auto *field = Field(form, "value");
                if (field == nullptr) {
                    ADD_FAILURE() << "Preferences page has no value field";
                    return 0;
                }
                const Any   value  = field->descriptor.GetValue();
                const auto *number = value.GetAsConst<int32_t>();
                if (number == nullptr) {
                    ADD_FAILURE() << "Preferences value has changed type";
                    return 0;
                }
                return *number;
            }

            PreferencePageRegistry   *pages = nullptr;
            UiTheme                   savedTheme;
            ui::UIBuiltinFontProvider font;
            ui::UITextSystem          text{&font, nullptr};
        };

    } // namespace

    TEST_F(PreferencesLayoutTest, SmallWindowAllocatesContentAndDistinctFooterButtonsBeforeFirstPaint)
    {
        PageData data;
        Add("page", data, "General settings");
        PreferencesDialog dialog(&text);
        dialog.SetModel(pages);
        dialog.Open();
        const ui::UIRect window{13.25f, 27.75f, 373.25f, 267.75f};
        dialog.ArrangeIfNeeded(window);
        ExpectInside(dialog.PanelRect(), window);
        ExpectInside(dialog.ContentRect(), dialog.PanelRect());
        ExpectInside(dialog.CategoryViewportRect(), dialog.ContentRect());
        ExpectInside(dialog.PageViewportRect(), dialog.ContentRect());
        EXPECT_TRUE(HasArea(dialog.CategoryViewportRect()));
        EXPECT_TRUE(HasArea(dialog.PageViewportRect()));
        EXPECT_FALSE(HasArea(Intersect(dialog.CategoryViewportRect(), dialog.PageViewportRect())));
        for (int index = 0; index != 4; ++index) {
            const auto button = dialog.ButtonRect(index);
            EXPECT_TRUE(HasArea(button));
            ExpectInside(button, dialog.PanelRect());
            EXPECT_FALSE(HasArea(Intersect(button, dialog.ContentRect())));
            const auto center = Pointer(ui::UIPointerAction::MOVE, button);
            EXPECT_EQ(dialog.ButtonAt(center.x, center.y), index);
            EXPECT_EQ(dialog.CategoryAt(center.x, center.y), -1);
            for (int other = index + 1; other != 4; ++other) {
                EXPECT_FALSE(HasArea(Intersect(button, dialog.ButtonRect(other))));
            }
        }
        const auto panel        = dialog.PanelRect();
        const auto pageViewport = dialog.PageViewportRect();
        const auto category     = dialog.CategoryRect(0);
        Paint(dialog);
        ExpectSameRect(dialog.PanelRect(), panel);
        ExpectSameRect(dialog.PageViewportRect(), pageViewport);
        ExpectSameRect(dialog.CategoryRect(0), category);
    }

    TEST_F(PreferencesLayoutTest, LongCategoryStaysSingleLineAndItsTextIsClippedToTheCategoryViewport)
    {
        PageData          first;
        PageData          second;
        const std::string longTitle = "Rendering and project configuration with a deliberately long category label for small windows";
        Add("long", first, longTitle);
        Add("short", second, "Audio");
        PreferencesDialog dialog(&text);
        dialog.SetModel(pages);
        dialog.Open();
        dialog.ArrangeIfNeeded({0.0f, 0.0f, 960.0f, 720.0f});
        const auto wideLong  = dialog.CategoryRect(0);
        const auto wideShort = dialog.CategoryRect(1);
        EXPECT_FLOAT_EQ(wideLong.Height(), wideShort.Height());
        EXPECT_GE(wideShort.top, wideLong.bottom);
        dialog.ArrangeIfNeeded({0.0f, 0.0f, 420.0f, 400.0f});
        const auto narrowLong = dialog.CategoryRect(0);
        EXPECT_LE(narrowLong.Width(), wideLong.Width());
        EXPECT_FLOAT_EQ(narrowLong.Height(), wideLong.Height());
        EXPECT_FLOAT_EQ(narrowLong.Height(), dialog.CategoryRect(1).Height());
        const auto visible = Intersect(narrowLong, dialog.CategoryViewportRect());
        ASSERT_TRUE(HasArea(visible));
        const auto center = Pointer(ui::UIPointerAction::MOVE, visible);
        EXPECT_EQ(dialog.CategoryAt(center.x, center.y), 0);
        const auto         before = narrowLong;
        ui::UIPaintContext paint;
        paint.Begin(dialog.GetBounds());
        dialog.Paint(paint);
        std::size_t glyphs = 0;
        for (const auto &command : paint.GetDrawData().commands) {
            if (command.textureId == ui::UI_INVALID_TEXTURE) {
                continue;
            }
            const float clipCenterY = (command.clip.top + command.clip.bottom) * 0.5f;
            if (clipCenterY >= narrowLong.top && clipCenterY < narrowLong.bottom && HasArea(Intersect(command.clip, dialog.CategoryViewportRect()))) {
                ExpectInside(command.clip, dialog.CategoryViewportRect());
                glyphs += command.indexCount / 6;
            }
        }
        EXPECT_GT(glyphs, 0u);
        const auto untruncatedGlyphs = std::count_if(longTitle.begin(), longTitle.end(), [](char character) { return character != ' '; });
        EXPECT_LT(glyphs, static_cast<std::size_t>(untruncatedGlyphs));
        ExpectSameRect(dialog.CategoryRect(0), before);
    }

    TEST_F(PreferencesLayoutTest, CategoryWheelReachesLatePagesWithoutSelectingRowsBehindTheFooter)
    {
        std::vector<PageData> data(48);
        for (std::size_t index = 0; index != data.size(); ++index) {
            Add("page" + std::to_string(index), data[index], "Settings " + std::to_string(index));
        }
        PreferencesDialog dialog(&text);
        dialog.SetModel(pages);
        dialog.Open();
        const ui::UIRect window{0.0f, 0.0f, 700.0f, 420.0f};
        dialog.ArrangeIfNeeded(window);
        ASSERT_EQ(dialog.GetPageCount(), static_cast<int>(data.size()));
        auto *form = Find<ReflectedFormView>(dialog);
        ASSERT_NE(form, nullptr);
        const auto  formBounds = form->GetBounds();
        const float firstTop   = dialog.CategoryRect(0).top;
        const auto  categories = dialog.CategoryViewportRect();
        dialog.OnPointerEvent(Pointer(ui::UIPointerAction::WHEEL, categories, -100000.0f));
        dialog.ArrangeIfNeeded(window);
        EXPECT_LT(dialog.CategoryRect(0).top, firstTop);
        ExpectSameRect(form->GetBounds(), formBounds);
        const int  lastIndex   = static_cast<int>(data.size()) - 1;
        const auto lastVisible = Intersect(dialog.CategoryRect(lastIndex), dialog.CategoryViewportRect());
        ASSERT_TRUE(HasArea(lastVisible));
        const auto lastCenter = Pointer(ui::UIPointerAction::MOVE, lastVisible);
        EXPECT_EQ(dialog.CategoryAt(lastCenter.x, lastCenter.y), lastIndex);
        Click(dialog, lastVisible);
        EXPECT_EQ(dialog.GetPageIndex(), lastIndex);
        for (int index = 0; index != 4; ++index) {
            const auto button = Pointer(ui::UIPointerAction::MOVE, dialog.ButtonRect(index));
            EXPECT_EQ(dialog.CategoryAt(button.x, button.y), -1);
        }
        const auto clipped      = dialog.CategoryRect(0);
        const auto hiddenCenter = Pointer(ui::UIPointerAction::MOVE, clipped);
        EXPECT_EQ(dialog.CategoryAt(hiddenCenter.x, hiddenCenter.y), -1);
    }

    TEST_F(PreferencesLayoutTest, ContentWheelRevealsAndEditsTheLastFieldWithoutMovingCategories)
    {
        PageData data;
        Add("dense", data);
        PreferencesDialog dialog(&text);
        dialog.SetModel(pages);
        dialog.Open();
        auto *form = Find<ReflectedFormView>(dialog);
        ASSERT_NE(form, nullptr);
        auto recorded = std::make_shared<RecordingBoolWidget>();
        form->Widgets().Register(PropertyEditorKind::Bool, recorded);
        const ui::UIRect window{0.0f, 0.0f, 680.0f, 360.0f};
        dialog.ArrangeIfNeeded(window);
        Paint(dialog);
        ASSERT_NE(recorded->controls.find("last"), recorded->controls.end());
        EXPECT_GE(recorded->controls.at("last").top, dialog.PageViewportRect().bottom);
        const auto  firstCategory  = dialog.CategoryRect(0);
        const auto  formBounds     = form->GetBounds();
        const float lastControlTop = recorded->controls.at("last").top;
        dialog.OnPointerEvent(Pointer(ui::UIPointerAction::WHEEL, dialog.PageViewportRect(), -100000.0f));
        dialog.ArrangeIfNeeded(window);
        Paint(dialog);
        ExpectSameRect(form->GetBounds(), formBounds);
        EXPECT_LT(recorded->controls.at("last").top, lastControlTop);
        ExpectSameRect(dialog.CategoryRect(0), firstCategory);
        const auto control = Intersect(recorded->controls.at("last"), dialog.PageViewportRect());
        ASSERT_TRUE(HasArea(control));
        Click(dialog, control);
        const auto *last = Field(*form, "last");
        ASSERT_NE(last, nullptr);
        const Any edited = last->descriptor.GetValue();
        ASSERT_NE(edited.GetAsConst<bool>(), nullptr);
        EXPECT_TRUE(*edited.GetAsConst<bool>());
        EXPECT_FALSE(data.last);
        EXPECT_EQ(dialog.History().GetUndoCount(), 1u);
        ASSERT_TRUE(dialog.History().Undo());
        const Any restored = Field(*form, "last")->descriptor.GetValue();
        ASSERT_NE(restored.GetAsConst<bool>(), nullptr);
        EXPECT_FALSE(*restored.GetAsConst<bool>());
    }

    TEST_F(PreferencesLayoutTest, ForeignPointerReleaseDoesNotEndTheCapturedFormDrag)
    {
        PageData data;
        Add("page", data);
        PreferencesDialog dialog(&text);
        dialog.SetModel(pages);
        dialog.Open();
        auto *form = Find<ReflectedFormView>(dialog);
        ASSERT_NE(form, nullptr);
        auto drag = std::make_shared<DragProbeWidget>();
        form->Widgets().Register(PropertyEditorKind::Bool, drag);
        dialog.ArrangeIfNeeded({0.0f, 0.0f, 1000.0f, 720.0f});
        Paint(dialog);
        ASSERT_NE(drag->controls.find("first"), drag->controls.end());
        const auto control = Intersect(drag->controls.at("first"), dialog.PageViewportRect());
        ASSERT_TRUE(HasArea(control));
        auto event      = Pointer(ui::UIPointerAction::DOWN, control);
        event.pointerId = 0;
        dialog.OnPointerEvent(event);
        ASSERT_TRUE(drag->dragging);
        ASSERT_EQ(drag->downs, 1);
        event.action    = ui::UIPointerAction::UP;
        event.pointerId = 1;
        dialog.OnPointerEvent(event);
        EXPECT_TRUE(drag->dragging);
        EXPECT_EQ(drag->ups, 0);
        event.action    = ui::UIPointerAction::MOVE;
        event.pointerId = 0;
        event.x         = -20.0f;
        event.y         = -20.0f;
        dialog.OnPointerEvent(event);
        EXPECT_EQ(drag->moves, 1);
        event.action = ui::UIPointerAction::UP;
        dialog.OnPointerEvent(event);
        EXPECT_EQ(drag->ups, 1);
        EXPECT_FALSE(drag->dragging);
    }

    TEST_F(PreferencesLayoutTest, PointerCancelClearsFormDragAndAllowsANewPointerToCapture)
    {
        PageData data;
        Add("page", data);
        PreferencesDialog dialog(&text);
        dialog.SetModel(pages);
        dialog.Open();
        auto *form = Find<ReflectedFormView>(dialog);
        ASSERT_NE(form, nullptr);
        auto drag = std::make_shared<DragProbeWidget>();
        form->Widgets().Register(PropertyEditorKind::Bool, drag);
        dialog.ArrangeIfNeeded({0.0f, 0.0f, 1000.0f, 720.0f});
        Paint(dialog);
        ASSERT_NE(drag->controls.find("first"), drag->controls.end());
        const auto control = Intersect(drag->controls.at("first"), dialog.PageViewportRect());
        ASSERT_TRUE(HasArea(control));
        auto event = Pointer(ui::UIPointerAction::DOWN, control);
        dialog.OnPointerEvent(event);
        ASSERT_TRUE(drag->dragging);
        event.action = ui::UIPointerAction::MOVE;
        event.x      = -20.0f;
        event.y      = -20.0f;
        dialog.OnPointerEvent(event);
        ASSERT_EQ(drag->moves, 1);
        dialog.OnPointerCancel(0);
        EXPECT_FALSE(drag->dragging);
        EXPECT_FALSE(form->WantsPointerCapture());
        EXPECT_EQ(drag->resets, 1);
        dialog.OnPointerEvent(event);
        EXPECT_EQ(drag->moves, 1);
        // A new pointer proves the dialog also released its private form
        // capture, rather than only resetting the widget's interaction state.
        event           = Pointer(ui::UIPointerAction::DOWN, control);
        event.pointerId = 1;
        dialog.OnPointerEvent(event);
        EXPECT_TRUE(drag->dragging);
        EXPECT_EQ(drag->downs, 2);
        event.action = ui::UIPointerAction::UP;
        event.x      = -20.0f;
        event.y      = -20.0f;
        dialog.OnPointerEvent(event);
        EXPECT_FALSE(drag->dragging);
        EXPECT_EQ(drag->ups, 1);
    }

    TEST_F(PreferencesLayoutTest, ResizeUpdatesCategoryAndFooterInputBeforeTheNextPaint)
    {
        PageData first;
        PageData second;
        Add("first", first, "First");
        Add("second", second, "Second");
        PreferencesDialog dialog(&text);
        dialog.SetModel(pages);
        dialog.Open();
        dialog.ArrangeIfNeeded({0.0f, 0.0f, 1000.0f, 760.0f});
        const auto wideButton = dialog.ButtonRect(2);
        dialog.ArrangeIfNeeded({0.0f, 0.0f, 410.0f, 330.0f});
        const auto secondRow = Intersect(dialog.CategoryRect(1), dialog.CategoryViewportRect());
        ASSERT_TRUE(HasArea(secondRow));
        Click(dialog, secondRow);
        EXPECT_EQ(dialog.GetPageIndex(), 1);
        int applied = 0;
        dialog.SetOnApplied([&applied]() { ++applied; });
        const auto apply = dialog.ButtonRect(2);
        EXPECT_TRUE(apply.left != wideButton.left || apply.top != wideButton.top);
        Click(dialog, apply);
        EXPECT_EQ(applied, 1);
        EXPECT_TRUE(dialog.IsOpen());
        Click(dialog, dialog.ButtonRect(1));
        EXPECT_FALSE(dialog.IsOpen());
    }

    TEST_F(PreferencesLayoutTest, SameScaleMetricsAndDpiChangesReachLayoutQueriesBeforePaint)
    {
        PageData data;
        Add("page", data, "General");
        PreferencesDialog dialog(&text);
        dialog.SetModel(pages);
        dialog.Open();
        const ui::UIRect window{0.0f, 0.0f, 1200.0f, 900.0f};
        dialog.SynchronizeLayout();
        dialog.ArrangeIfNeeded(window);
        const auto initialCategory = dialog.CategoryRect(0);
        const auto initialButton   = dialog.ButtonRect(2);
        UiTheme    changed         = GetDefaultUiTheme();
        changed.metrics.categoryRowHeight += 17.0f;
        changed.metrics.frameHeight += 11.0f;
        SetDefaultUiTheme(changed);
        dialog.SynchronizeLayout();
        dialog.ArrangeIfNeeded(window);
        EXPECT_GT(dialog.CategoryRect(0).Height(), initialCategory.Height());
        EXPECT_GT(dialog.ButtonRect(2).Height(), initialButton.Height());
        const auto changedRow = Intersect(dialog.CategoryRect(0), dialog.CategoryViewportRect());
        ASSERT_TRUE(HasArea(changedRow));
        auto center = Pointer(ui::UIPointerAction::MOVE, changedRow);
        EXPECT_EQ(dialog.CategoryAt(center.x, center.y), 0);
        const auto sameScaleButton = dialog.ButtonRect(2);
        SetDefaultUiTheme(MakeDarkTheme(1.5f));
        dialog.SynchronizeLayout();
        dialog.ArrangeIfNeeded(window);
        EXPECT_NE(dialog.ButtonRect(2).Width(), sameScaleButton.Width());
        ExpectInside(dialog.PanelRect(), window);
        for (int index = 0; index != 4; ++index) {
            const auto button = dialog.ButtonRect(index);
            ExpectInside(button, dialog.PanelRect());
            center = Pointer(ui::UIPointerAction::MOVE, button);
            EXPECT_EQ(dialog.ButtonAt(center.x, center.y), index);
        }
        const auto beforePaint = dialog.PageViewportRect();
        Paint(dialog);
        ExpectSameRect(dialog.PageViewportRect(), beforePaint);
    }

    TEST_F(PreferencesLayoutTest, DegenerateWindowsKeepQueriesFiniteAndDoNotExposeHiddenTargets)
    {
        PageData data;
        Add("page", data);
        PreferencesDialog dialog(&text);
        dialog.SetModel(pages);
        dialog.Open();
        for (const auto window : {ui::UIRect{0.0f, 0.0f, 0.0f, 0.0f}, ui::UIRect{0.0f, 0.0f, 1.0f, 1.0f}, ui::UIRect{0.0f, 0.0f, 80.0f, 60.0f},
                                  ui::UIRect{0.0f, 0.0f, 180.0f, 120.0f}}) {
            dialog.ArrangeIfNeeded(window);
            ExpectInside(dialog.PanelRect(), window);
            ExpectInside(dialog.ContentRect(), dialog.PanelRect());
            ExpectInside(dialog.CategoryViewportRect(), dialog.ContentRect());
            ExpectInside(dialog.PageViewportRect(), dialog.ContentRect());
            for (int index = 0; index != 4; ++index) {
                const auto button = dialog.ButtonRect(index);
                ExpectInside(button, dialog.PanelRect());
                const auto center = Pointer(ui::UIPointerAction::MOVE, button);
                if (HasArea(button)) {
                    EXPECT_EQ(dialog.ButtonAt(center.x, center.y), index);
                } else {
                    EXPECT_NE(dialog.ButtonAt(center.x, center.y), index);
                }
            }
            if (!HasArea(dialog.CategoryViewportRect())) {
                const auto center = Pointer(ui::UIPointerAction::MOVE, dialog.CategoryViewportRect());
                EXPECT_EQ(dialog.CategoryAt(center.x, center.y), -1);
            }
        }
        const ui::UIRect restoredWindow{0.0f, 0.0f, 1000.0f, 760.0f};
        dialog.ArrangeIfNeeded(restoredWindow);
        ExpectInside(dialog.PanelRect(), restoredWindow);
        EXPECT_TRUE(HasArea(dialog.CategoryViewportRect()));
        EXPECT_TRUE(HasArea(dialog.PageViewportRect()));
        const auto firstButton = dialog.ButtonRect(0);
        for (int index = 0; index != 4; ++index) {
            const auto button = dialog.ButtonRect(index);
            EXPECT_TRUE(HasArea(button));
            ExpectInside(button, dialog.PanelRect());
            EXPECT_FLOAT_EQ(button.top, firstButton.top);
            EXPECT_FLOAT_EQ(button.bottom, firstButton.bottom);
            const auto center = Pointer(ui::UIPointerAction::MOVE, button);
            EXPECT_EQ(dialog.ButtonAt(center.x, center.y), index);
        }
    }

    TEST_F(PreferencesLayoutTest, PaintConsumesExistingGeometryAndLeavesPageReconciliationToLayout)
    {
        PageData first;
        PageData second;
        Add("first", first);
        PreferencesDialog dialog(&text);
        dialog.SetModel(pages);
        dialog.Open();
        dialog.ArrangeIfNeeded({0.0f, 0.0f, 900.0f, 680.0f});
        auto *form = Find<ReflectedFormView>(dialog);
        ASSERT_NE(form, nullptr);
        const auto  categoryBounds = dialog.CategoryRect(0);
        const auto *category       = FindLeafAtBounds(dialog, categoryBounds);
        ASSERT_NE(category, nullptr);
        const auto formBounds  = form->GetBounds();
        const auto beforePaint = Snapshot(dialog);
        Paint(dialog);
        ExpectSameTree(dialog, beforePaint);
        EXPECT_EQ(Find<ReflectedFormView>(dialog), form);
        EXPECT_EQ(FindLeafAtBounds(dialog, categoryBounds), category);
        ExpectSameRect(form->GetBounds(), formBounds);

        Add("second", second);
        // Do not call layout/hit-query helpers here: they explicitly synchronize.
        // Paint must consume the previous page snapshot until the host lays out.
        EXPECT_EQ(dialog.GetPageCount(), 1);
        Paint(dialog);
        EXPECT_EQ(dialog.GetPageCount(), 1);
        ExpectSameTree(dialog, beforePaint);
        EXPECT_EQ(Find<ReflectedFormView>(dialog), form);
        EXPECT_EQ(FindLeafAtBounds(dialog, categoryBounds), category);
        dialog.SynchronizeLayout();
        EXPECT_EQ(dialog.GetPageCount(), 2);
        EXPECT_GT(Snapshot(dialog).size(), beforePaint.size());
        EXPECT_EQ(Find<ReflectedFormView>(dialog), form);
        EXPECT_TRUE(HasArea(dialog.CategoryRect(1)));
    }

    TEST_F(PreferencesLayoutTest, FormPopupOwnsCategoryAndFooterInputAndCanPaintBeyondTheModalPanel)
    {
        PageData first;
        PageData second;
        Add("first", first);
        Add("second", second);
        PreferencesDialog dialog(&text);
        dialog.SetModel(pages);
        int applied = 0;
        int closed  = 0;
        dialog.SetOnApplied([&applied]() { ++applied; });
        dialog.SetOnClosed([&closed]() { ++closed; });
        dialog.Open();
        auto *form = Find<ReflectedFormView>(dialog);
        ASSERT_NE(form, nullptr);
        auto popup = std::make_shared<PopupProbeWidget>();
        form->Widgets().Register(PropertyEditorKind::Bool, popup);
        const ui::UIRect window{0.0f, 0.0f, 1000.0f, 760.0f};
        dialog.ArrangeIfNeeded(window);
        Paint(dialog);
        ASSERT_NE(popup->controls.find("first"), popup->controls.end());
        const auto control = Intersect(popup->controls.at("first"), dialog.PageViewportRect());
        ASSERT_TRUE(HasArea(control));
        dialog.OnPointerEvent(Pointer(ui::UIPointerAction::DOWN, control));
        ASSERT_TRUE(popup->open);
        ASSERT_TRUE(form->WantsPointerCapture());
        ui::UIPaintContext paint;
        paint.Begin(window);
        dialog.Paint(paint);
        ExpectSameRect(popup->hostBounds, window);
        ExpectInside(popup->popup, window);
        EXPECT_LT(popup->popup.left, dialog.PanelRect().left);
        EXPECT_GT(popup->popup.right, dialog.PanelRect().right);
        bool        markerDrawn = false;
        const auto &draw        = paint.GetDrawData();
        for (const auto &command : draw.commands) {
            for (uint32_t offset = 0; offset != command.indexCount; ++offset) {
                const auto vertex = draw.indices[command.indexOffset + offset];
                if (draw.vertices[vertex].color == PopupProbeWidget::PopupColor) {
                    markerDrawn = true;
                    ExpectInside(popup->popup, command.clip);
                    ExpectInside(command.clip, window);
                    break;
                }
            }
        }
        EXPECT_TRUE(markerDrawn);
        const auto category = Intersect(dialog.CategoryRect(1), dialog.CategoryViewportRect());
        ASSERT_TRUE(HasArea(Intersect(category, popup->popup)));
        Click(dialog, category);
        EXPECT_EQ(dialog.GetPageIndex(), 0);
        EXPECT_EQ(popup->downs, 1);
        EXPECT_EQ(popup->ups, 1);
        Click(dialog, dialog.ButtonRect(2));
        EXPECT_EQ(applied, 0);
        EXPECT_EQ(popup->downs, 2);
        EXPECT_EQ(popup->ups, 2);
        Click(dialog, dialog.ButtonRect(1));
        EXPECT_TRUE(dialog.IsOpen());
        EXPECT_EQ(closed, 0);
        EXPECT_EQ(popup->downs, 3);
        EXPECT_EQ(popup->ups, 3);
        ui::UIKeyEvent escape;
        escape.keyCode = 0x1B;
        dialog.OnKeyEvent(escape);
        EXPECT_FALSE(popup->open);
        EXPECT_FALSE(form->WantsPointerCapture());
        EXPECT_TRUE(dialog.IsOpen());
    }

    TEST_F(PreferencesLayoutTest, LargeOriginsPreserveThePanelsUnpositionedLogicalAllocation)
    {
        UiTheme theme                        = GetDefaultUiTheme();
        theme.metrics.preferencesPanelWidth  = 880.3f;
        theme.metrics.preferencesPanelHeight = 600.2f;
        SetDefaultUiTheme(theme);
        PageData data;
        Add("page", data);
        PreferencesDialog dialog(&text);
        dialog.SetModel(pages);
        dialog.Open();
        // Positioned float edges lose fractional extents here. The host's
        // explicit logical allocation remains authoritative for composition.
        const ui::UIRect window{10000000.0f, -10000000.0f, 10001000.0f, -9999200.0f};
        dialog.ArrangeIfNeeded(window, ui::UISize{1000.0f, 800.0f});
        ui::DockPanel *panel = nullptr;
        for (const auto &child : dialog.GetChildren()) {
            if (auto *dock = dynamic_cast<ui::DockPanel *>(child.get())) {
                panel = dock;
                break;
            }
        }
        ASSERT_NE(panel, nullptr);
        EXPECT_FLOAT_EQ(dialog.GetArrangedSize().width, 1000.0f);
        EXPECT_FLOAT_EQ(dialog.GetArrangedSize().height, 800.0f);
        EXPECT_NE(panel->GetBounds().Width(), theme.metrics.preferencesPanelWidth);
        EXPECT_NE(panel->GetBounds().Height(), theme.metrics.preferencesPanelHeight);
        EXPECT_FLOAT_EQ(panel->GetArrangedSize().width, theme.metrics.preferencesPanelWidth);
        EXPECT_FLOAT_EQ(panel->GetArrangedSize().height, theme.metrics.preferencesPanelHeight);
        dialog.SynchronizeLayout();
        Paint(dialog);
        EXPECT_FLOAT_EQ(panel->GetArrangedSize().width, theme.metrics.preferencesPanelWidth);
        EXPECT_FLOAT_EQ(panel->GetArrangedSize().height, theme.metrics.preferencesPanelHeight);
    }

    TEST_F(PreferencesLayoutTest, ScrolledSelectionApplyCancelAndLocalHistoryKeepSessionSemantics)
    {
        std::vector<PageData> data(36);
        for (std::size_t index = 0; index != data.size(); ++index) {
            Add("page" + std::to_string(index), data[index]);
        }
        PageData           unrelated;
        const auto        *type = GetTypeNode(TypeInfo<PageData>::RegisteredId());
        PropertyDescriptor globalValue{&unrelated, &type->members.at("value"), "value", ""};
        EditorCore::GetCommandService().Execute(globalValue.MakeEditCommand(Any(int32_t{42})));
        PreferencesDialog dialog(&text);
        dialog.SetModel(pages);
        dialog.Open();
        const ui::UIRect window{0.0f, 0.0f, 700.0f, 430.0f};
        dialog.ArrangeIfNeeded(window);
        dialog.OnPointerEvent(Pointer(ui::UIPointerAction::WHEEL, dialog.CategoryViewportRect(), -100000.0f));
        dialog.ArrangeIfNeeded(window);
        const int  lastIndex = static_cast<int>(data.size()) - 1;
        const auto visible   = Intersect(dialog.CategoryRect(lastIndex), dialog.CategoryViewportRect());
        ASSERT_TRUE(HasArea(visible));
        Click(dialog, visible);
        ASSERT_EQ(dialog.GetPageIndex(), lastIndex);
        auto *form = Find<ReflectedFormView>(dialog);
        ASSERT_NE(form, nullptr);
        auto *value = Field(*form, "value");
        ASSERT_NE(value, nullptr);
        ASSERT_TRUE(form->Form().Edit(*value, Any(int32_t{7}), dialog.History()));
        EXPECT_EQ(data.back().value, 1);
        ASSERT_TRUE(dialog.History().Undo());
        EXPECT_EQ(WorkingValue(*form), 1);
        ASSERT_TRUE(dialog.History().Redo());
        EXPECT_EQ(WorkingValue(*form), 7);
        dialog.ArrangeIfNeeded(window);
        Click(dialog, dialog.ButtonRect(2));
        EXPECT_EQ(data.back().value, 7);
        EXPECT_FALSE(dialog.History().CanUndo());
        ASSERT_TRUE(dialog.IsOpen());
        value = Field(*form, "value");
        ASSERT_NE(value, nullptr);
        ASSERT_TRUE(form->Form().Edit(*value, Any(int32_t{9}), dialog.History()));
        dialog.ArrangeIfNeeded(window);
        Click(dialog, dialog.ButtonRect(1));
        EXPECT_FALSE(dialog.IsOpen());
        EXPECT_EQ(data.back().value, 7);
        EXPECT_FALSE(dialog.History().Undo());
        EXPECT_EQ(EditorCore::GetCommandService().GetUndoCount(), 1u);
        EXPECT_EQ(unrelated.value, 42);
        dialog.Open();
        dialog.ArrangeIfNeeded(window);
        EXPECT_EQ(dialog.GetPageIndex(), 0);
        EXPECT_FALSE(dialog.History().CanUndo());
        const auto first = Intersect(dialog.CategoryRect(0), dialog.CategoryViewportRect());
        EXPECT_TRUE(HasArea(first));
    }

} // namespace sky::editor::preferences_layout_test
