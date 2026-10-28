#include "ReflectionTestEnvironment.h"
//
// Headless coverage for reusable reflected widgets and document service contexts.
//

#include <core/math/Transform.h>
#include <core/math/Vector2.h>
#include <core/type/TypeInfo.h>
#include <editor/core/EditorCore.h>
#include <editor/core/input/KeyModifiers.h>
#include <editor/widgets/AssetReflectedWidget.h>
#include <editor/widgets/ColorPicker.h>
#include <editor/widgets/GenericReflectedWidget.h>
#include <editor/widgets/ReflectedFormView.h>
#include <editor/widgets/RotationReflectedWidget.h>
#include <framework/serialization/SerializationContext.h>
#include <ui/IUIFontProvider.h>
#include <ui/IUITextureRegistry.h>
#include <ui/UIContext.h>
#include <ui/UIEventRouter.h>
#include <ui/UIPaintContext.h>
#include <ui/text/UITextSystem.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <vector>

namespace sky::editor::widget_test {

    enum class Choice : uint32_t { First, Second };

    struct Color {
        float r = 0.2f;
        float g = 0.4f;
        float b = 0.6f;
        float a = 1.0f;
    };

    struct Object {
        Choice      choice  = Choice::First;
        bool        enabled = false;
        std::string text    = "initial";
        Color       color;
        Vector2     position;
    };

    struct ToggleObject {
        bool enabled = false;
    };

    struct AssetObject {
        Uuid mesh;
    };

    class AssetCatalog final : public IEditorAssetCatalog {
    public:
        Uuid asset = Uuid::Create();
        std::vector<EditorAssetItem> Gather(const std::string &type) const override
        {
            return {{asset, "Character", "Character.gltf", type}};
        }
        bool GetType(const Uuid &id, std::string &out) const override
        {
            out = "AuroraMesh";
            return id == asset;
        }
        bool GetName(const Uuid &id, std::string &out) const override
        {
            out = "Character";
            return id == asset;
        }
    };

    struct LayoutObject {
        ToggleObject       nested;
        std::vector<float> items;
    };

    struct MetadataToggleObject {
        bool alpha  = false;
        bool zeta   = false;
        bool hidden = false;
    };

    namespace {

        const auto *reflectionEnvironment = sky::editor::test::AddReflectionEnvironment([] {
            auto *serialization = SerializationContext::Get();
            serialization->Register<Choice>("ReflectedWidgetTestChoice").Enum(Choice::First, "First").Enum(Choice::Second, "Second");
            serialization->Register<Color>("ReflectedWidgetTestColor")
                .Member<&Color::r>("r")
                .Member<&Color::g>("g")
                .Member<&Color::b>("b")
                .Member<&Color::a>("a");
            serialization->Register<Object>("ReflectedWidgetTestObject")
                .Member<&Object::choice>("choice")
                .Member<&Object::enabled>("enabled")
                .Member<&Object::text>("text")
                .Member<&Object::color>("color")
                .Member<&Object::position>("position");
            serialization->Register<ToggleObject>("ReflectedWidgetTestToggle").Member<&ToggleObject::enabled>("enabled");
            serialization->Register<AssetObject>("ReflectedWidgetTestAsset")
                .Member<&AssetObject::mesh>("mesh")
                .Property(CommonPropertyKey::ASSET_TYPE, Any(std::string("AuroraMesh")));
            serialization->Register<LayoutObject>("ReflectedWidgetTestLayout")
                .Member<&LayoutObject::nested>("nested")
                .Member<&LayoutObject::items>("items");
            serialization->Register<MetadataToggleObject>("ReflectedWidgetMetadataToggle")
                .Member<&MetadataToggleObject::alpha>("alpha")
                .Property(CommonPropertyKey::ORDER, Any(int32_t{10}))
                .Member<&MetadataToggleObject::zeta>("zeta")
                .Property(CommonPropertyKey::ORDER, Any(int32_t{-10}))
                .Member<&MetadataToggleObject::hidden>("hidden")
                .Property(CommonPropertyKey::VISIBLE, Any(false));
        });

        const TypeNode *ObjectType()
        {
            return GetTypeNode(TypeInfo<Object>::RegisteredId());
        }

        struct Services {
            CommandService         commands;
            PropertyEditorRegistry registry;
            PropertyChangeNotifier changes;
            UiTheme                theme = MakeDarkTheme();

            Services()
            {
                registry.RegisterType(TypeInfo<Color>::RegisteredId(), PropertyEditorKind::Color);
            }

            ReflectedFormContext Context()
            {
                return {commands, registry, changes, theme};
            }
        };

        PropertyField *Field(ReflectedFormView &view, const char *path)
        {
            for (auto &section : view.Form().GetSections()) {
                for (auto &field : section.fields) {
                    if (field.path == path) {
                        return &field;
                    }
                }
            }
            return nullptr;
        }

        void Paint(ReflectedFormView &view)
        {
            ui::UIPaintContext paint;
            paint.Begin(view.GetBounds());
            view.Paint(paint);
        }

        ui::UIPointerEvent Pointer(ui::UIPointerAction action, float x, float y)
        {
            ui::UIPointerEvent event;
            event.action = action;
            event.x      = x;
            event.y      = y;
            return event;
        }

        class ComponentTestFont : public ui::IUIFontProvider {
        public:
            bool GetGlyph(uint32_t, uint32_t size, ui::UIGlyphBitmap &out) override
            {
                out          = {};
                out.advance  = static_cast<float>(size);
                out.width    = size;
                out.height   = size;
                out.bearingY = static_cast<float>(size);
                out.pixels.assign(static_cast<size_t>(size) * size, 255);
                return true;
            }

            ui::UIFontMetrics GetFontMetrics(uint32_t size) override
            {
                return {static_cast<float>(size), 0.0f, static_cast<float>(size)};
            }
        };

        class ComponentProbeForm : public ReflectedFormView {
        public:
            using ReflectedFormView::ReflectedFormView;

            void
            BeginTextEdit(PropertyField &, int component, PropertyEditorKind, const std::string &, std::function<bool(const std::string &)>) override
            {
                editedComponent = component;
            }

            int editedComponent = -1;
        };

        struct ComponentTextRun {
            ui::UIRect clip;
            ui::UIRect ink;
            uint32_t   glyphs = 0;
        };

        std::vector<ComponentTextRun> ComponentText(const ui::UIDrawData &data, uint32_t color)
        {
            std::vector<ComponentTextRun> result;
            for (const auto &command : data.commands) {
                if (command.textureId == ui::UI_INVALID_TEXTURE || command.indexCount == 0)
                    continue;
                const auto &first = data.vertices[data.indices[command.indexOffset]];
                if (first.color != color)
                    continue;
                ComponentTextRun run{command.clip, {first.x, first.y, first.x, first.y}, command.indexCount / 6};
                for (uint32_t i = 0; i < command.indexCount; ++i) {
                    const auto &vertex = data.vertices[data.indices[command.indexOffset + i]];
                    run.ink.left       = std::min(run.ink.left, vertex.x);
                    run.ink.right      = std::max(run.ink.right, vertex.x);
                    run.ink.top        = std::min(run.ink.top, vertex.y);
                    run.ink.bottom     = std::max(run.ink.bottom, vertex.y);
                }
                result.push_back(run);
            }
            return result;
        }

        void CheckComponentPaintAndHits(
            ReflectedWidget &widget, ComponentProbeForm &host, PropertyField &field, const ui::UIRect &rect, int count, bool modeButton)
        {
            const auto        &theme = host.Skin().Theme();
            ui::UIPaintContext paint;
            paint.Begin({0, 0, 2000, 1600});
            widget.Paint(host, paint, field, rect);
            const auto labels = ComponentText(paint.GetDrawData(), theme.colors.textMuted);
            const auto values = ComponentText(paint.GetDrawData(), theme.colors.text);
            ASSERT_EQ(labels.size(), static_cast<size_t>(count + (modeButton ? 1 : 0)));
            ASSERT_EQ(values.size(), static_cast<size_t>(count));
            const float y = (rect.top + rect.bottom) * 0.5f;
            for (int i = 0; i < count; ++i) {
                SCOPED_TRACE(i);
                EXPECT_EQ(labels[i].glyphs, 1u);
                EXPECT_FLOAT_EQ(labels[i].ink.Height(), static_cast<float>(theme.fonts.tiny));
                EXPECT_GE(values[i].clip.left - labels[i].ink.right, theme.metrics.controlPad - 1.0f);
                EXPECT_GE(values[i].glyphs, 3u);
                EXPECT_TRUE(widget.OnDown(host, field, Pointer(ui::UIPointerAction::DOWN, labels[i].ink.left, y), rect));
                EXPECT_EQ(host.editedComponent, i);
                const float valueX = (values[i].ink.left + values[i].ink.right) * 0.5f;
                EXPECT_TRUE(widget.OnDown(host, field, Pointer(ui::UIPointerAction::DOWN, valueX, y), rect));
                EXPECT_EQ(host.editedComponent, i);
                EXPECT_FALSE(widget.OnDown(host, field, Pointer(ui::UIPointerAction::DOWN, valueX, rect.bottom + 1.0f), rect));
                if (i + 1 < count) {
                    const float gap = labels[i + 1].ink.left - values[i].clip.right - 2.0f * theme.metrics.controlPad;
                    EXPECT_NEAR(gap, theme.metrics.itemSpacing, 1.0f);
                    const float gapX = (values[i].clip.right + labels[i + 1].ink.left) * 0.5f;
                    EXPECT_FALSE(widget.OnDown(host, field, Pointer(ui::UIPointerAction::DOWN, gapX, y), rect));
                }
            }
        }

        class LayoutProbeWidget : public ReflectedWidget {
        public:
            explicit LayoutProbeWidget(PropertyEditorKind inKind) : kind(inKind)
            {
            }
            PropertyEditorKind Kind() const override
            {
                return kind;
            }
            int RowCount() const override
            {
                return rowCount;
            }
            void Paint(ReflectedWidgetHost &, ui::UIPaintContext &, PropertyField &, const ui::UIRect &rect) override
            {
                lastRect = rect;
                ++paints;
            }

            PropertyEditorKind kind;
            ui::UIRect         lastRect;
            int                rowCount = 1;
            int                paints   = 0;
        };

        class LayoutProbeForm : public ReflectedFormView {
        public:
            using ReflectedFormView::ReflectedFormView;
            int arrangements = 0;
            int ticks        = 0;

        protected:
            void ArrangeChildren(const ui::UIRect &content) override
            {
                ++arrangements;
                ReflectedFormView::ArrangeChildren(content);
            }
            void OnViewTick() override
            {
                ++ticks;
            }
        };

        struct SessionStats {
            int releases     = 0;
            int resets       = 0;
            int ups          = 0;
            int commits      = 0;
            int afterRefresh = 0;
        };

        class InlineWidget : public ReflectedWidget {
        public:
            explicit InlineWidget(std::shared_ptr<SessionStats> stats) : stats(std::move(stats))
            {
            }
            ~InlineWidget() override
            {
                ++stats->releases;
            }
            PropertyEditorKind Kind() const override
            {
                return PropertyEditorKind::Bool;
            }
            void Paint(ReflectedWidgetHost &, ui::UIPaintContext &, PropertyField &, const ui::UIRect &) override
            {
            }
            bool OnDown(ReflectedWidgetHost &host, PropertyField &field, const ui::UIPointerEvent &, const ui::UIRect &) override
            {
                host.BeginTextEdit(field, -1, PropertyEditorKind::String, "", [this, view = &host](const std::string &) {
                    ++stats->commits;
                    view->RefreshForm();
                    // Refresh clears the view's editCommit; this callback still
                    // needs its widget until it returns to CommitEdit.
                    ++stats->afterRefresh;
                    return true;
                });
                return true;
            }

        private:
            std::shared_ptr<SessionStats> stats;
        };

        class ReplaceableWidget : public ReflectedWidget {
        public:
            explicit ReplaceableWidget(std::shared_ptr<SessionStats> stats) : stats(std::move(stats))
            {
            }
            ~ReplaceableWidget() override
            {
                ++stats->releases;
            }
            PropertyEditorKind Kind() const override
            {
                return PropertyEditorKind::Bool;
            }
            void Paint(ReflectedWidgetHost &, ui::UIPaintContext &, PropertyField &, const ui::UIRect &) override
            {
            }
            bool OnDown(ReflectedWidgetHost &, PropertyField &, const ui::UIPointerEvent &, const ui::UIRect &) override
            {
                popup = true;
                return true;
            }
            bool HasPopup() const override
            {
                return popup;
            }
            bool OnPopupPointer(ReflectedWidgetHost &host, const ui::UIPointerEvent &event) override
            {
                if (event.action == ui::UIPointerAction::UP) {
                    ++stats->ups;
                    popup = false;
                    host.RefreshForm();
                }
                return true;
            }
            void ResetInteraction(ReflectedWidgetHost &, ReflectedInteractionReset) override
            {
                popup = false;
                ++stats->resets;
            }

        private:
            std::shared_ptr<SessionStats> stats;
            bool                          popup = false;
        };

        class TextureRegistry : public ui::IUITextureRegistry {
        public:
            ui::UITextureId RegisterTexture(const ui::UIImageData &image) override
            {
                return RegisterTextureAs(next++, image);
            }
            ui::UITextureId RegisterTextureAs(ui::UITextureId id, const ui::UIImageData &image) override
            {
                if (images.emplace(id, image).second) {
                    ++creations;
                }
                return id;
            }
            void UpdateTexture(ui::UITextureId id, const ui::UIImageData &image) override
            {
                images[id] = image;
            }
            void ReleaseTexture(ui::UITextureId id) override
            {
                images.erase(id);
            }

            std::unordered_map<ui::UITextureId, ui::UIImageData> images;
            int                                                  creations = 0;

        private:
            ui::UITextureId next = 1;
        };

    } // namespace

    TEST(ReflectedFormWidgetsTest, AssetPickerSurvivesOpeningReleaseAndSelectionIsUndoable)
    {
        ObjectType();
        AssetObject object;
        AssetCatalog catalog;
        Services services;
        struct Probe final : AssetReflectedWidget {
            explicit Probe(IEditorAssetCatalog *catalog) : AssetReflectedWidget(catalog) {}
            ui::UIRect control;
            void Paint(ReflectedWidgetHost &host, ui::UIPaintContext &context, PropertyField &field, const ui::UIRect &rect) override
            {
                control = rect;
                AssetReflectedWidget::Paint(host, context, field, rect);
            }
        };
        auto widget = std::make_shared<Probe>(&catalog);
        ReflectedFormView view(nullptr, "", services.Context());
        view.Widgets().Register(PropertyEditorKind::Asset, widget);
        view.Bind({&object, GetTypeNode(TypeInfo<AssetObject>::RegisteredId())});
        view.Arrange({0, 0, 800, 600});
        Paint(view);
        const auto control = widget->control;
        const float x = (control.left + control.right) * .5f;
        const float y = (control.top + control.bottom) * .5f;
        view.OnPointerEvent(Pointer(ui::UIPointerAction::DOWN, x, y));
        ASSERT_TRUE(view.WantsPointerCapture());
        view.OnPointerEvent(Pointer(ui::UIPointerAction::UP, x, y));
        ASSERT_TRUE(view.WantsPointerCapture());
        EXPECT_FALSE(object.mesh);
        const auto &metrics = services.theme.metrics;
        const float itemY = control.bottom + metrics.checkboxPad * 2.5f + metrics.popupItemHeight * .5f;
        view.OnPointerEvent(Pointer(ui::UIPointerAction::DOWN, x, itemY));
        view.OnPointerEvent(Pointer(ui::UIPointerAction::UP, x, itemY));
        EXPECT_EQ(object.mesh, catalog.asset);
        EXPECT_FALSE(view.WantsPointerCapture());
        ASSERT_EQ(services.commands.GetUndoCount(), 1u);
        ASSERT_TRUE(services.commands.Undo());
        EXPECT_FALSE(object.mesh);
    }

    TEST(ReflectedFormWidgetsTest, GroupedFormsKeepExpansionAndHistoryIndependent)
    {
        ObjectType();
        const auto          *type = GetTypeNode(TypeInfo<LayoutObject>::RegisteredId());
        LayoutObject         first;
        LayoutObject         second;
        Services             services;
        ReflectedFormView    view(nullptr, "Inspector", services.Context());
        const PropertyObject objects[]{{&first, type}, {&second, type}};
        view.BindGroups(objects);
        auto *a = Field(view, "[0].nested");
        auto *b = Field(view, "[1].nested");
        ASSERT_NE(a, nullptr);
        ASSERT_NE(b, nullptr);
        a->expanded = true;
        b->expanded = false;
        view.Refresh();
        a = Field(view, "[0].nested");
        b = Field(view, "[1].nested");
        ASSERT_NE(a, nullptr);
        ASSERT_NE(b, nullptr);
        EXPECT_TRUE(a->expanded);
        EXPECT_FALSE(b->expanded);
        ASSERT_EQ(b->children.size(), 1u);
        ASSERT_TRUE(view.Form().Edit(b->children[0], Any(true), services.commands));
        EXPECT_FALSE(first.nested.enabled);
        EXPECT_TRUE(second.nested.enabled);
        ASSERT_TRUE(services.commands.Undo());
        view.SynchronizeLayout();
        EXPECT_FALSE(second.nested.enabled);
        EXPECT_EQ(view.Form().GetSections().size(), 2u);
        view.Unbind();
        services.changes.Notify();
        view.SynchronizeLayout();
        EXPECT_TRUE(view.Form().GetSections().empty());
    }

    TEST(ReflectedFormWidgetsTest, FormsOwnIndependentWidgetInteraction)
    {
        const TypeNode *type = ObjectType();
        ASSERT_NE(type, nullptr);
        Object            first;
        Object            second;
        Services          services;
        ReflectedFormView a(nullptr, "A", services.Context());
        ReflectedFormView b(nullptr, "B", services.Context());
        a.Bind({&first, type});
        b.Bind({&second, type});
        a.Arrange({0.0f, 0.0f, 800.0f, 600.0f});
        b.Arrange(a.GetBounds());
        auto *aEnum = a.Widgets().Find(PropertyEditorKind::Enum);
        auto *bEnum = b.Widgets().Find(PropertyEditorKind::Enum);
        ASSERT_NE(aEnum, nullptr);
        ASSERT_NE(bEnum, nullptr);
        EXPECT_NE(aEnum, bEnum);
        ASSERT_NE(Field(a, "choice"), nullptr);
        ASSERT_NE(Field(b, "choice"), nullptr);
        EXPECT_TRUE(aEnum->OnDown(a, *Field(a, "choice"), Pointer(ui::UIPointerAction::DOWN, 150.0f, 30.0f), {100.0f, 20.0f, 300.0f, 40.0f}));
        EXPECT_TRUE(a.WantsPointerCapture());
        EXPECT_FALSE(b.WantsPointerCapture());
        Paint(b);
        EXPECT_TRUE(a.WantsPointerCapture());
        EXPECT_FALSE(b.WantsPointerCapture());

        EXPECT_TRUE(bEnum->OnDown(b, *Field(b, "choice"), Pointer(ui::UIPointerAction::DOWN, 150.0f, 30.0f), {100.0f, 20.0f, 300.0f, 40.0f}));
        a.Refresh();
        EXPECT_FALSE(a.WantsPointerCapture());
        EXPECT_TRUE(b.WantsPointerCapture());
        b.Bind({});
        EXPECT_FALSE(b.WantsPointerCapture());
    }

    TEST(ReflectedFormWidgetsTest, InjectedCommandHistoriesStayIndependent)
    {
        const TypeNode   *type = ObjectType();
        Object            first;
        Object            second;
        Services          firstServices;
        Services          secondServices;
        ReflectedFormView a(nullptr, "A", firstServices.Context());
        ReflectedFormView b(nullptr, "B", secondServices.Context());
        a.Bind({&first, type});
        b.Bind({&second, type});
        auto toggle = [](ReflectedFormView &view) {
            PropertyField *field = Field(view, "enabled");
            ASSERT_NE(field, nullptr);
            auto *widget = view.Widgets().Find(PropertyEditorKind::Bool);
            ASSERT_NE(widget, nullptr);
            EXPECT_TRUE(widget->OnDown(view, *field, Pointer(ui::UIPointerAction::DOWN, 1.0f, 1.0f), {}));
        };
        toggle(a);
        EXPECT_TRUE(first.enabled);
        EXPECT_FALSE(second.enabled);
        EXPECT_EQ(firstServices.commands.GetUndoCount(), 1u);
        EXPECT_EQ(secondServices.commands.GetUndoCount(), 0u);
        toggle(b);
        ASSERT_TRUE(a.Commands().Undo());
        EXPECT_FALSE(first.enabled);
        EXPECT_TRUE(second.enabled);
        EXPECT_EQ(secondServices.commands.GetUndoCount(), 1u);
    }

    TEST(ReflectedFormWidgetsTest, EnumPopupUsesInjectedThemeForHitTesting)
    {
        const TypeNode *type = ObjectType();
        Object          object;
        Services        services;
        services.theme.metrics.checkboxPad     = 5.0f;
        services.theme.metrics.popupItemHeight = 60.0f;
        ReflectedFormView view(nullptr, "", services.Context());
        view.Bind({&object, type});
        view.Arrange({0.0f, 0.0f, 800.0f, 600.0f});
        EXPECT_EQ(&view.Skin().Theme(), &services.theme);
        auto *widget = view.Widgets().Find(PropertyEditorKind::Enum);
        ASSERT_NE(widget, nullptr);
        ASSERT_NE(Field(view, "choice"), nullptr);
        widget->OnDown(view, *Field(view, "choice"), Pointer(ui::UIPointerAction::DOWN, 150.0f, 30.0f), {100.0f, 20.0f, 300.0f, 40.0f});
        // Theme-specific second row is y=[110,170); the default popup is much shorter.
        EXPECT_EQ(view.OnPointerEvent(Pointer(ui::UIPointerAction::DOWN, 150.0f, 140.0f)), ui::UIEventResult::HANDLED);
        EXPECT_EQ(object.choice, Choice::Second);
        EXPECT_EQ(services.commands.GetUndoCount(), 1u);
        EXPECT_FALSE(view.WantsPointerCapture());
    }

    TEST(ReflectedFormWidgetsTest, ExplicitCancellationRestoresLiveColorBeforeRebindOrDestruction)
    {
        const TypeNode *type = ObjectType();
        Object          first;
        Object          second;
        Services        services;
        auto            view = std::make_unique<ReflectedFormView>(nullptr, "", services.Context());
        view->Bind({&first, type});
        view->Arrange({0.0f, 0.0f, 800.0f, 600.0f});
        auto openPreview = [](ReflectedFormView &form) {
            auto *widget = form.Widgets().Find(PropertyEditorKind::Color);
            ASSERT_NE(widget, nullptr);
            ASSERT_NE(Field(form, "color"), nullptr);
            widget->OnDown(form, *Field(form, "color"), Pointer(ui::UIPointerAction::DOWN, 101.0f, 35.0f), {100.0f, 30.0f, 400.0f, 60.0f});
            // The hue wheel for this anchor is centered at (282,144), radius 60.
            form.OnPointerEvent(Pointer(ui::UIPointerAction::DOWN, 312.0f, 144.0f));
            EXPECT_TRUE(form.WantsPointerCapture());
        };
        openPreview(*view);
        EXPECT_NE(first.color.r, 0.2f);
        view->CancelInteractions();
        view->Bind({&second, type});
        EXPECT_FLOAT_EQ(first.color.r, 0.2f);
        EXPECT_FLOAT_EQ(first.color.g, 0.4f);
        EXPECT_FLOAT_EQ(first.color.b, 0.6f);
        EXPECT_FALSE(view->WantsPointerCapture());
        EXPECT_FLOAT_EQ(second.color.r, 0.2f);
        openPreview(*view);
        EXPECT_NE(second.color.r, 0.2f);
        view->CancelInteractions();
        view.reset();
        EXPECT_FLOAT_EQ(second.color.r, 0.2f);
        EXPECT_EQ(services.commands.GetUndoCount(), 0u);
    }

    TEST(ReflectedFormWidgetsTest, RebindingAbandonsPreviewWithoutWritingOldModel)
    {
        const TypeNode   *type = ObjectType();
        Object            first;
        Object            second;
        Services          services;
        ReflectedFormView view(nullptr, "", services.Context());
        view.Bind({&first, type});
        view.Arrange({0.0f, 0.0f, 800.0f, 600.0f});
        auto *widget = view.Widgets().Find(PropertyEditorKind::Color);
        ASSERT_NE(widget, nullptr);
        ASSERT_NE(Field(view, "color"), nullptr);
        ASSERT_TRUE(widget->OnDown(view, *Field(view, "color"), Pointer(ui::UIPointerAction::DOWN, 101.0f, 35.0f), {100.0f, 30.0f, 400.0f, 60.0f}));
        view.OnPointerEvent(Pointer(ui::UIPointerAction::DOWN, 312.0f, 144.0f));
        const Color preview = first.color;
        ASSERT_NE(preview.r, 0.2f);
        const auto oldLifetime = view.BindingLifetime();

        view.Bind({&second, type});
        EXPECT_TRUE(oldLifetime.expired());
        EXPECT_FLOAT_EQ(first.color.r, preview.r);
        EXPECT_FLOAT_EQ(first.color.g, preview.g);
        EXPECT_FLOAT_EQ(first.color.b, preview.b);
        EXPECT_FLOAT_EQ(second.color.r, 0.2f);
        EXPECT_FALSE(view.WantsPointerCapture());
        view.OnPointerEvent(Pointer(ui::UIPointerAction::UP, 312.0f, 144.0f));
        EXPECT_EQ(services.commands.GetUndoCount(), 0u);
    }

    TEST(ReflectedFormWidgetsTest, InvalidModelCanBeUnboundReboundOrDestroyedDuringPreview)
    {
        const TypeNode *type = ObjectType();
        Services        services;
        auto            view           = std::make_unique<ReflectedFormView>(nullptr, "", services.Context());
        auto            bindAndPreview = [&](Object &object) {
            view->Bind({&object, type});
            view->Arrange({0.0f, 0.0f, 800.0f, 600.0f});
            auto *widget = view->Widgets().Find(PropertyEditorKind::Color);
            ASSERT_NE(widget, nullptr);
            ASSERT_NE(Field(*view, "color"), nullptr);
            ASSERT_TRUE(
                widget->OnDown(*view, *Field(*view, "color"), Pointer(ui::UIPointerAction::DOWN, 101.0f, 35.0f), {100.0f, 30.0f, 400.0f, 60.0f}));
            view->OnPointerEvent(Pointer(ui::UIPointerAction::DOWN, 312.0f, 144.0f));
            ASSERT_NE(object.color.r, 0.2f);
        };

        auto first = std::make_unique<Object>();
        bindAndPreview(*first);
        first.reset(); // obsolete descriptor and rollback callback now point at released storage
        view->Unbind();
        EXPECT_FALSE(view->Form().IsValid());
        EXPECT_FALSE(view->WantsPointerCapture());
        services.changes.Notify();
        view->SynchronizeLayout(); // no deferred refresh may resurrect the obsolete binding
        EXPECT_FALSE(view->Form().IsValid());

        auto second = std::make_unique<Object>();
        bindAndPreview(*second);
        second.reset();
        auto third = std::make_unique<Object>();
        view->Bind({third.get(), type});
        EXPECT_FLOAT_EQ(third->color.r, 0.2f);
        bindAndPreview(*third);
        const auto lastLifetime = view->BindingLifetime();
        third.reset();
        view.reset();
        EXPECT_TRUE(lastLifetime.expired());
        services.changes.Notify();
        EXPECT_EQ(services.commands.GetUndoCount(), 0u);
    }

    TEST(ReflectedFormWidgetsTest, EscapeStillRestoresLiveColorPreview)
    {
        const TypeNode   *type = ObjectType();
        Object            object;
        Services          services;
        ReflectedFormView view(nullptr, "", services.Context());
        view.Bind({&object, type});
        view.Arrange({0.0f, 0.0f, 800.0f, 600.0f});
        auto *widget = view.Widgets().Find(PropertyEditorKind::Color);
        ASSERT_NE(widget, nullptr);
        ASSERT_NE(Field(view, "color"), nullptr);
        ASSERT_TRUE(widget->OnDown(view, *Field(view, "color"), Pointer(ui::UIPointerAction::DOWN, 101.0f, 35.0f), {100.0f, 30.0f, 400.0f, 60.0f}));
        view.OnPointerEvent(Pointer(ui::UIPointerAction::DOWN, 312.0f, 144.0f));
        ASSERT_NE(object.color.r, 0.2f);
        ui::UIKeyEvent escape;
        escape.keyCode = 0x1B;
        escape.action  = ui::UIKeyAction::DOWN;
        EXPECT_EQ(view.OnKeyEvent(escape), ui::UIEventResult::HANDLED);
        EXPECT_FLOAT_EQ(object.color.r, 0.2f);
        EXPECT_FLOAT_EQ(object.color.g, 0.4f);
        EXPECT_FLOAT_EQ(object.color.b, 0.6f);
        EXPECT_FALSE(view.WantsPointerCapture());
        EXPECT_EQ(services.commands.GetUndoCount(), 0u);
    }

    TEST(ReflectedFormWidgetsTest, SingularSurfaceCancelsCapturedColorPreviewWithoutCommitting)
    {
        const TypeNode *type = ObjectType();
        ASSERT_NE(type, nullptr);
        Object        object;
        const Color   original = object.color;
        Services      services;
        ui::UIContext context;
        context.SetContentSize(800.0f, 600.0f);
        auto               holder = std::make_unique<ReflectedFormView>(nullptr, "", services.Context());
        ReflectedFormView *view   = holder.get();
        context.AddChild(std::move(holder));
        view->Bind({&object, type});
        view->Arrange(context.GetRoot()->GetBounds());

        auto *widget = view->Widgets().Find(PropertyEditorKind::Color);
        ASSERT_NE(widget, nullptr);
        ASSERT_NE(Field(*view, "color"), nullptr);
        ASSERT_TRUE(widget->OnDown(*view, *Field(*view, "color"), Pointer(ui::UIPointerAction::DOWN, 101.0f, 35.0f), {100.0f, 30.0f, 400.0f, 60.0f}));
        ui::UIEventRouter router(context);
        ASSERT_EQ(router.DispatchPointer(Pointer(ui::UIPointerAction::DOWN, 312.0f, 144.0f)), ui::UIEventResult::HANDLED);
        ASSERT_EQ(router.GetCapture(), view);
        ASSERT_TRUE(view->WantsPointerCapture());
        ASSERT_NE(object.color.r, original.r); // the routed press started a live preview
        EXPECT_EQ(services.commands.GetUndoCount(), 0u);

        ui::UI2DTransform singular;
        singular.m00 = 0.0f;
        context.SetSurfaceTransform(singular);
        EXPECT_EQ(router.DispatchPointer(Pointer(ui::UIPointerAction::UP, 312.0f, 144.0f)), ui::UIEventResult::UNHANDLED);
        EXPECT_FLOAT_EQ(object.color.r, original.r);
        EXPECT_FLOAT_EQ(object.color.g, original.g);
        EXPECT_FLOAT_EQ(object.color.b, original.b);
        EXPECT_FLOAT_EQ(object.color.a, original.a);
        EXPECT_FALSE(view->WantsPointerCapture());
        EXPECT_EQ(services.commands.GetUndoCount(), 0u);
        EXPECT_EQ(router.GetCapture(), nullptr);

        context.SetSurfaceTransform(ui::UI2DTransform::Identity());
        ui::UIPaintContext paint;
        context.Paint(paint);
        EXPECT_FALSE(paint.GetDrawData().vertices.empty());
    }

    TEST(ReflectedFormWidgetsTest, ColorCommitCanRefreshItsOwnFormAndUndo)
    {
        const TypeNode   *type = ObjectType();
        Object            object;
        Services          services;
        ReflectedFormView view(nullptr, "", services.Context());
        view.Bind({&object, type});
        view.Arrange({0.0f, 0.0f, 800.0f, 600.0f});
        auto *widget = view.Widgets().Find(PropertyEditorKind::Color);
        ASSERT_NE(widget, nullptr);
        ASSERT_NE(Field(view, "color"), nullptr);
        widget->OnDown(view, *Field(view, "color"), Pointer(ui::UIPointerAction::DOWN, 101.0f, 35.0f), {100.0f, 30.0f, 400.0f, 60.0f});
        view.OnPointerEvent(Pointer(ui::UIPointerAction::DOWN, 312.0f, 144.0f));
        view.OnPointerEvent(Pointer(ui::UIPointerAction::UP, 312.0f, 144.0f));
        EXPECT_FALSE(view.WantsPointerCapture());
        EXPECT_NE(object.color.r, 0.2f);
        ASSERT_EQ(services.commands.GetUndoCount(), 1u);
        ASSERT_TRUE(services.commands.Undo());
        EXPECT_FLOAT_EQ(object.color.r, 0.2f);
        EXPECT_FLOAT_EQ(object.color.g, 0.4f);
        EXPECT_FLOAT_EQ(object.color.b, 0.6f);
    }

    TEST(ReflectedFormWidgetsTest, ExternalChangesWaitForEditThenRefreshWithoutAnotherRevision)
    {
        const TypeNode   *type = ObjectType();
        Object            object;
        Object            other;
        Services          services;
        Services          otherServices;
        ReflectedFormView view(nullptr, "", services.Context());
        ReflectedFormView otherView(nullptr, "", otherServices.Context());
        view.Bind({&object, type});
        otherView.Bind({&other, type});
        view.Arrange({0.0f, 0.0f, 800.0f, 600.0f});
        otherView.Arrange(view.GetBounds());
        PropertyField *field = Field(view, "text");
        ASSERT_NE(field, nullptr);
        view.BeginTextEdit(*field, -1, PropertyEditorKind::String, "pending", [](const std::string &) { return true; });
        services.registry.RegisterMemberAppearance(TypeInfo<Object>::RegisteredId(), "text", {"Updated", 0, false});
        otherServices.registry.RegisterMemberAppearance(TypeInfo<Object>::RegisteredId(), "text", {"Other", 0, false});
        services.changes.Notify();
        Paint(view);
        Paint(otherView);
        ASSERT_NE(Field(view, "text"), nullptr);
        EXPECT_EQ(Field(view, "text")->label, "text");
        EXPECT_EQ(Field(otherView, "text")->label, "text");
        ui::UIKeyEvent escape;
        escape.keyCode = 0x1B;
        view.OnKeyEvent(escape);
        Paint(view);
        EXPECT_EQ(Field(view, "text")->label, "Updated");
        EXPECT_EQ(Field(otherView, "text")->label, "text");
    }

    TEST(ReflectedFormWidgetsTest, PreferredHeightIsReadyBeforePaintAndHeaderBindInvalidateAncestors)
    {
        ObjectType();
        ToggleObject  object;
        Services      services;
        ui::UIElement host;
        host.SetLayoutVolatile(false);
        auto  owned = std::make_unique<LayoutProbeForm>(nullptr, "", services.Context());
        auto *view  = static_cast<LayoutProbeForm *>(host.AddChild(std::move(owned)));
        view->SetLayoutVolatile(false);
        ui::UILayoutParams fill;
        fill.width.mode  = ui::UISizeMode::FILL;
        fill.height.mode = ui::UISizeMode::FILL;
        view->SetLayout(fill);
        view->SetHeaderVisible(false);
        view->Bind({&object, GetTypeNode(TypeInfo<ToggleObject>::RegisteredId())});
        const float preferred = view->GetPreferredHeight();
        EXPECT_FLOAT_EQ(preferred, services.theme.metrics.sectionHeight + services.theme.metrics.rowHeight + 32.0f);
        host.ArrangeIfNeeded({0.0f, 0.0f, 400.0f, 300.0f});
        ASSERT_FALSE(host.IsLayoutDirty());
        ASSERT_FALSE(view->IsLayoutDirty());
        const auto bounds = view->GetBounds();
        EXPECT_FLOAT_EQ(view->MeasureDesired().height, preferred);
        EXPECT_FLOAT_EQ(view->GetPreferredHeight(), preferred);
        EXPECT_FLOAT_EQ(view->GetBounds().Width(), bounds.Width());
        EXPECT_FALSE(view->IsLayoutDirty());
        EXPECT_FALSE(host.IsLayoutDirty());
        Paint(*view);
        EXPECT_EQ(view->ticks, 0);
        EXPECT_FLOAT_EQ(view->GetPreferredHeight(), preferred);
        EXPECT_FALSE(host.IsLayoutDirty());

        const int arrangements = view->arrangements;
        host.ArrangeIfNeeded(host.GetBounds());
        EXPECT_EQ(view->arrangements, arrangements);
        view->SetHeaderVisible(true);
        EXPECT_TRUE(host.IsLayoutDirty());
        EXPECT_FLOAT_EQ(view->GetPreferredHeight(), preferred + services.theme.metrics.headerHeight);
        host.ArrangeIfNeeded(host.GetBounds());
        view->SetHeaderVisible(true);
        EXPECT_FALSE(host.IsLayoutDirty());
        view->Bind({});
        EXPECT_TRUE(host.IsLayoutDirty());
        EXPECT_FLOAT_EQ(view->GetPreferredHeight(), services.theme.metrics.headerHeight + 24.0f);
        host.ArrangeIfNeeded(host.GetBounds());
        view->Bind({&object, GetTypeNode(TypeInfo<ToggleObject>::RegisteredId())});
        EXPECT_TRUE(host.IsLayoutDirty());
        EXPECT_FLOAT_EQ(view->GetPreferredHeight(), preferred + services.theme.metrics.headerHeight);
    }

    TEST(ReflectedFormWidgetsTest, NestedExpansionAndResizeUseArrangedRowsBeforeAnyPaintRefresh)
    {
        ObjectType();
        LayoutObject object;
        Services     services;
        services.registry.RegisterMemberAppearance(TypeInfo<LayoutObject>::RegisteredId(), "nested", {"Nested", 0, true});
        services.registry.RegisterMemberAppearance(TypeInfo<LayoutObject>::RegisteredId(), "items", {"Items", 1, true});
        ui::UIElement host;
        host.SetLayoutVolatile(false);
        auto  owned = std::make_unique<ReflectedFormView>(nullptr, "", services.Context());
        auto *view  = static_cast<ReflectedFormView *>(host.AddChild(std::move(owned)));
        view->SetLayoutVolatile(false);
        ui::UILayoutParams fill;
        fill.width.mode  = ui::UISizeMode::FILL;
        fill.height.mode = ui::UISizeMode::FILL;
        view->SetLayout(fill);
        view->SetHeaderVisible(false);
        view->Bind({&object, GetTypeNode(TypeInfo<LayoutObject>::RegisteredId())});
        auto probe = std::make_shared<LayoutProbeWidget>(PropertyEditorKind::Bool);
        view->Widgets().Register(PropertyEditorKind::Bool, probe);
        const float collapsed = view->GetPreferredHeight();
        host.ArrangeIfNeeded({0.0f, 0.0f, 400.0f, 300.0f});
        const float firstRowY = 4.0f + services.theme.metrics.sectionHeight + services.theme.metrics.rowHeight * 0.5f;
        view->OnPointerEvent(Pointer(ui::UIPointerAction::DOWN, services.theme.metrics.padX + 8.0f, firstRowY));
        ASSERT_NE(Field(*view, "nested"), nullptr);
        EXPECT_TRUE(Field(*view, "nested")->expanded);
        EXPECT_TRUE(host.IsLayoutDirty());
        EXPECT_FLOAT_EQ(view->GetPreferredHeight(), collapsed + services.theme.metrics.rowHeight);
        host.ArrangeIfNeeded(host.GetBounds());
        Paint(*view);
        ASSERT_EQ(probe->paints, 1);
        const float originalWidth = probe->lastRect.Width();
        host.ArrangeIfNeeded({0.0f, 0.0f, 600.0f, 300.0f});
        Paint(*view);
        EXPECT_GT(probe->lastRect.Width(), originalWidth);
        EXPECT_FLOAT_EQ(view->GetPreferredHeight(), collapsed + services.theme.metrics.rowHeight);
        EXPECT_FALSE(host.IsLayoutDirty());
        view->OnPointerEvent(Pointer(ui::UIPointerAction::DOWN, services.theme.metrics.padX + 8.0f, firstRowY));
        EXPECT_FALSE(Field(*view, "nested")->expanded);
        EXPECT_TRUE(host.IsLayoutDirty());
        EXPECT_FLOAT_EQ(view->GetPreferredHeight(), collapsed);
    }

    TEST(ReflectedFormWidgetsTest, MetadataOrderDrivesActualRowsAndEditsWithoutLosingLabelOverrides)
    {
        ObjectType();
        MetadataToggleObject object;
        Services             services;
        services.registry.RegisterMemberAppearance(TypeInfo<MetadataToggleObject>::RegisteredId(), "zeta", {"First toggle", 99, false});
        ReflectedFormView view(nullptr, "", services.Context());
        view.SetHeaderVisible(false);
        view.Bind({&object, GetTypeNode(TypeInfo<MetadataToggleObject>::RegisteredId())});
        class RowProbe : public GenericReflectedWidget {
        public:
            void Paint(ReflectedWidgetHost &host, ui::UIPaintContext &context, PropertyField &field, const ui::UIRect &rect) override
            {
                bounds[field.path] = rect;
                GenericReflectedWidget::Paint(host, context, field, rect);
            }
            std::unordered_map<std::string, ui::UIRect> bounds;
        };
        auto probe = std::make_shared<RowProbe>();
        view.Widgets().Register(PropertyEditorKind::Bool, probe);
        view.Arrange({0.0f, 0.0f, 500.0f, 300.0f});
        Paint(view);
        ASSERT_EQ(probe->bounds.size(), 2u);
        EXPECT_LT(probe->bounds.at("zeta").top, probe->bounds.at("alpha").top);
        EXPECT_EQ(Field(view, "zeta")->label, "First toggle");
        const auto  rect = probe->bounds.at("zeta");
        const float x    = (rect.left + rect.right) * 0.5f;
        const float y    = (rect.top + rect.bottom) * 0.5f;
        view.OnPointerEvent(Pointer(ui::UIPointerAction::DOWN, x, y));
        view.OnPointerEvent(Pointer(ui::UIPointerAction::UP, x, y));
        EXPECT_TRUE(object.zeta);
        EXPECT_FALSE(object.alpha);
        EXPECT_FALSE(object.hidden);
        ASSERT_TRUE(services.commands.Undo());
        EXPECT_FALSE(object.zeta);
    }

    TEST(ReflectedFormWidgetsTest, SequenceButtonsAndExternalUndoSynchronizeHeightBeforePaint)
    {
        ObjectType();
        LayoutObject      object;
        Services          services;
        ReflectedFormView view(nullptr, "", services.Context());
        view.SetLayoutVolatile(false);
        view.SetHeaderVisible(false);
        view.Bind({&object, GetTypeNode(TypeInfo<LayoutObject>::RegisteredId())});
        auto probe = std::make_shared<LayoutProbeWidget>(PropertyEditorKind::Sequence);
        view.Widgets().Register(PropertyEditorKind::Sequence, probe);
        int edits = 0;
        view.SetOnEdited([&edits]() { ++edits; });
        const float initial = view.GetPreferredHeight();
        view.ArrangeIfNeeded({0.0f, 0.0f, 500.0f, 400.0f});
        Paint(view);
        ASSERT_EQ(probe->paints, 1);
        const auto plus = Pointer(ui::UIPointerAction::DOWN, probe->lastRect.right - 8.0f, (probe->lastRect.top + probe->lastRect.bottom) * 0.5f);
        view.OnPointerEvent(plus);
        ASSERT_EQ(object.items.size(), 1u);
        EXPECT_EQ(edits, 1);
        ASSERT_NE(Field(view, "items"), nullptr);
        EXPECT_TRUE(Field(view, "items")->expanded);
        ASSERT_EQ(Field(view, "items")->children.size(), 1u);
        EXPECT_FALSE(Field(view, "items")->children.front().descriptor.IsSequence());
        EXPECT_EQ(Field(view, "items")->children.front().kind, PropertyEditorKind::Float);
        EXPECT_TRUE(view.IsLayoutDirty());
        EXPECT_FLOAT_EQ(view.GetPreferredHeight(), initial + services.theme.metrics.rowHeight);
        view.ArrangeIfNeeded(view.GetBounds());
        Paint(view);
        ASSERT_EQ(probe->paints, 2);
        view.OnPointerEvent(Pointer(ui::UIPointerAction::DOWN, probe->lastRect.right - 28.0f, (probe->lastRect.top + probe->lastRect.bottom) * 0.5f));
        ASSERT_TRUE(object.items.empty());
        EXPECT_EQ(edits, 2);
        EXPECT_FLOAT_EQ(view.GetPreferredHeight(), initial);
        view.ArrangeIfNeeded(view.GetBounds());
        ASSERT_FALSE(view.IsLayoutDirty());

        ASSERT_TRUE(services.commands.Undo());
        ASSERT_EQ(object.items.size(), 1u);
        EXPECT_TRUE(view.IsLayoutDirty());
        // The notification never rebuilds fields from inside command dispatch.
        EXPECT_TRUE(Field(view, "items")->children.empty());
        view.SynchronizeLayout();
        EXPECT_EQ(Field(view, "items")->children.size(), 1u);
        EXPECT_TRUE(Field(view, "items")->expanded);
        EXPECT_FLOAT_EQ(view.GetPreferredHeight(), initial + services.theme.metrics.rowHeight);
        EXPECT_EQ(edits, 2);
        ASSERT_TRUE(services.commands.Redo());
        view.SynchronizeLayout();
        EXPECT_TRUE(Field(view, "items")->children.empty());
        EXPECT_FLOAT_EQ(view.GetPreferredHeight(), initial);
    }

    TEST(ReflectedFormWidgetsTest, RetainedFormSynchronizesNotifiedChangesOutsidePaintAndDefersActiveEdit)
    {
        const TypeNode   *type = ObjectType();
        Object            object;
        Services          services;
        ReflectedFormView view(nullptr, "", services.Context());
        view.SetLayoutVolatile(false);
        view.Bind({&object, type});
        view.ArrangeIfNeeded({0.0f, 0.0f, 800.0f, 600.0f});
        services.registry.RegisterMemberAppearance(TypeInfo<Object>::RegisteredId(), "text", {"Updated", 0, false});
        services.changes.Notify();
        EXPECT_TRUE(view.IsLayoutDirty());
        view.GetPreferredHeight();
        Paint(view);
        EXPECT_EQ(Field(view, "text")->label, "text");
        view.SynchronizeLayout();
        EXPECT_EQ(Field(view, "text")->label, "Updated");
        view.ArrangeIfNeeded(view.GetBounds());
        auto *field = Field(view, "text");
        ASSERT_NE(field, nullptr);
        view.BeginTextEdit(*field, -1, PropertyEditorKind::String, "pending", [](const std::string &) { return true; });
        services.registry.RegisterMemberAppearance(TypeInfo<Object>::RegisteredId(), "text", {"After edit", 0, false});
        services.changes.Notify();
        view.SynchronizeLayout();
        EXPECT_EQ(Field(view, "text"), field);
        EXPECT_EQ(field->label, "Updated");
        ui::UIKeyEvent escape;
        escape.keyCode = 0x1B;
        view.OnKeyEvent(escape);
        view.SynchronizeLayout();
        EXPECT_EQ(Field(view, "text")->label, "After edit");
    }

    TEST(ReflectedFormWidgetsTest, DefaultVolatileTicksAndExplicitWidgetRefreshRemainCompatible)
    {
        ObjectType();
        ToggleObject    object;
        Services        services;
        LayoutProbeForm view(nullptr, "", services.Context());
        EXPECT_TRUE(view.IsLayoutVolatile());
        view.Bind({&object, GetTypeNode(TypeInfo<ToggleObject>::RegisteredId())});
        view.Arrange({0.0f, 0.0f, 400.0f, 300.0f});
        Paint(view);
        EXPECT_EQ(view.ticks, 1);
        view.SetLayoutVolatile(false);
        auto probe = std::make_shared<LayoutProbeWidget>(PropertyEditorKind::Bool);
        view.Widgets().Register(PropertyEditorKind::Bool, probe);
        view.Refresh();
        const float oneRow = view.GetPreferredHeight();
        view.ArrangeIfNeeded(view.GetBounds());
        probe->rowCount = 3;
        view.Refresh();
        EXPECT_TRUE(view.IsLayoutDirty());
        EXPECT_FLOAT_EQ(view.GetPreferredHeight(), oneRow + services.theme.metrics.rowHeight * 2.0f);
        view.ArrangeIfNeeded(view.GetBounds());
        Paint(view);
        EXPECT_EQ(view.ticks, 1);
        EXPECT_FLOAT_EQ(probe->lastRect.Height(), services.theme.metrics.rowHeight * 3.0f - 2.0f);
    }

    TEST(ReflectedFormWidgetsTest, ObserverCanDestroyFormDuringNotifiedOrCommandCallbackSnapshot)
    {
        ObjectType();
        for (bool commandNotification : {false, true}) {
            for (bool registerBeforeForm : {false, true}) {
                Services                           services;
                ToggleObject                       object;
                std::unique_ptr<ReflectedFormView> view;
                int                                destructions = 0;
                const auto                         destroy      = [&]() {
                    if (view) {
                        view.reset();
                        ++destructions;
                    }
                };
                int                        propertyObserver = 0;
                CommandService::CallbackId commandObserver  = 0;
                const auto                 subscribe        = [&]() {
                    if (commandNotification) {
                        commandObserver = services.commands.AddChangeCallback(destroy);
                    } else {
                        propertyObserver = services.changes.Subscribe(destroy);
                    }
                };
                if (registerBeforeForm) {
                    subscribe();
                }
                view = std::make_unique<ReflectedFormView>(nullptr, "", services.Context());
                view->Bind({&object, GetTypeNode(TypeInfo<ToggleObject>::RegisteredId())});
                if (!registerBeforeForm) {
                    subscribe();
                }
                // Both services snapshot closures before dispatch. Exercise
                // both registration orders without assuming hash-map order.
                for (int notification = 0; notification != 2; ++notification) {
                    if (commandNotification) {
                        services.commands.Clear();
                    } else {
                        services.changes.Notify();
                    }
                    EXPECT_EQ(view, nullptr);
                    EXPECT_EQ(destructions, 1);
                }
                if (commandNotification) {
                    services.commands.RemoveChangeCallback(commandObserver);
                } else {
                    services.changes.Unsubscribe(propertyObserver);
                }
            }
        }
    }

    TEST(ReflectedFormWidgetsTest, RegistryReplacementInvalidatesRetainedDesiredSizeAndArrangedRows)
    {
        ObjectType();
        ToggleObject  object;
        Services      services;
        ui::UIElement host;
        host.SetLayoutVolatile(false);
        auto  owned = std::make_unique<ReflectedFormView>(nullptr, "", services.Context());
        auto *view  = static_cast<ReflectedFormView *>(host.AddChild(std::move(owned)));
        view->SetLayoutVolatile(false);
        ui::UILayoutParams fill;
        fill.width.mode  = ui::UISizeMode::FILL;
        fill.height.mode = ui::UISizeMode::FILL;
        view->SetLayout(fill);
        view->Bind({&object, GetTypeNode(TypeInfo<ToggleObject>::RegisteredId())});
        auto oneRow = std::make_shared<LayoutProbeWidget>(PropertyEditorKind::Bool);
        view->Widgets().Register(PropertyEditorKind::Bool, oneRow);
        const float initialHeight = view->MeasureDesired().height;
        host.ArrangeIfNeeded({0.0f, 0.0f, 400.0f, 300.0f});
        Paint(*view);
        ASSERT_FALSE(host.IsLayoutDirty());
        auto threeRows      = std::make_shared<LayoutProbeWidget>(PropertyEditorKind::Bool);
        threeRows->rowCount = 3;
        view->Widgets().Register(PropertyEditorKind::Bool, threeRows);
        EXPECT_TRUE(host.IsLayoutDirty());
        EXPECT_TRUE(view->IsLayoutDirty());
        EXPECT_FLOAT_EQ(view->MeasureDesired().height, initialHeight + services.theme.metrics.rowHeight * 2.0f);
        host.ArrangeIfNeeded(host.GetBounds());
        Paint(*view);
        EXPECT_EQ(oneRow->paints, 1);
        EXPECT_EQ(threeRows->paints, 1);
        EXPECT_FLOAT_EQ(threeRows->lastRect.Height(), services.theme.metrics.rowHeight * 3.0f - 2.0f);
        EXPECT_FALSE(host.IsLayoutDirty());
    }

    TEST(ReflectedFormWidgetsTest, TwoTimesVectorAxesKeepTheirGlyphsAndValueGutters)
    {
        Transform object;
        object.translation = {30.0f, 30.0f, 30.0f};
        Services services;
        services.theme = MakeDarkTheme(2.0f);
        ComponentTestFont font;
        ui::UITextSystem text(&font, nullptr);
        ComponentProbeForm host(&text, "", services.Context());
        host.Bind({&object, GetTypeNode(TypeInfo<Transform>::RegisteredId())});
        auto *field = Field(host, "translation");
        ASSERT_NE(field, nullptr);
        ASSERT_EQ(field->kind, PropertyEditorKind::Vector);
        GenericReflectedWidget widget;
        CheckComponentPaintAndHits(widget, host, *field, {100, 100, 1300, 100 + services.theme.metrics.frameHeight}, 3, false);
    }

    TEST(ReflectedFormWidgetsTest, TwoTimesRotationModesShareMeasuredAxesAndPointerBounds)
    {
        Transform object;
        Services  services;
        services.theme = MakeDarkTheme(2.0f);
        ComponentTestFont  font;
        ui::UITextSystem   text(&font, nullptr);
        ComponentProbeForm host(&text, "", services.Context());
        host.Bind({&object, GetTypeNode(TypeInfo<Transform>::RegisteredId())});
        auto *field = Field(host, "rotation");
        ASSERT_NE(field, nullptr);
        ASSERT_EQ(field->kind, PropertyEditorKind::Rotation);
        RotationReflectedWidget widget;
        const ui::UIRect        rect{100, 100, 1300, 100 + services.theme.metrics.frameHeight};
        CheckComponentPaintAndHits(widget, host, *field, rect, 3, true);
        ui::UIPaintContext paint;
        paint.Begin({0, 0, 2000, 1600});
        widget.Paint(host, paint, *field, rect);
        const auto labels = ComponentText(paint.GetDrawData(), services.theme.colors.textMuted);
        ASSERT_FALSE(labels.empty());
        const auto &buttonText = labels.back().ink;
        EXPECT_TRUE(widget.OnDown(
            host, *field, Pointer(ui::UIPointerAction::DOWN, (buttonText.left + buttonText.right) * 0.5f, (rect.top + rect.bottom) * 0.5f), rect));
        CheckComponentPaintAndHits(widget, host, *field, rect, 4, true);
    }

    TEST(ReflectedFormWidgetsTest, EveryRegistryGetsDefaultsWithoutOverwritingExtensions)
    {
        Object     object;
        Services   first;
        Services   second;
        const Uuid vectorType    = TypeInfo<Vector2>::RegisteredId();
        const Uuid transformType = TypeInfo<Transform>::RegisteredId();
        first.registry.RegisterType(vectorType, PropertyEditorKind::Color);
        first.registry.RegisterMemberAppearance(transformType, "translation", {"Offset", 42, true});
        ReflectedFormView a(nullptr, "", first.Context());
        ReflectedFormView b(nullptr, "", second.Context());
        ReflectedFormView repeated(nullptr, "", first.Context());
        const auto       *appearance = first.registry.GetMemberAppearance(transformType, "translation");
        ASSERT_NE(appearance, nullptr);
        EXPECT_EQ(appearance->label, "Offset");
        EXPECT_EQ(appearance->order, 42);
        ASSERT_NE(second.registry.GetMemberAppearance(transformType, "translation"), nullptr);
        EXPECT_EQ(second.registry.GetMemberAppearance(transformType, "translation")->label, "Position");
        EXPECT_TRUE(second.registry.HasType(vectorType));
        a.Bind({&object, ObjectType()});
        ASSERT_NE(Field(a, "position"), nullptr);
        EXPECT_EQ(Field(a, "position")->kind, PropertyEditorKind::Color);
    }

    TEST(ReflectedFormWidgetsTest, HistoryCancelsLivePreviewBeforeUndoAndRedo)
    {
        const TypeNode   *type = ObjectType();
        Object            object;
        Services          services;
        ReflectedFormView view(nullptr, "", services.Context());
        view.Bind({&object, type});
        view.Arrange({0.0f, 0.0f, 800.0f, 600.0f});
        Color edited;
        edited.r = 0.8f;
        edited.g = 0.2f;
        edited.b = 0.1f;
        ASSERT_NE(Field(view, "color"), nullptr);
        ASSERT_TRUE(view.Form().Edit(*Field(view, "color"), Any(edited), services.commands));
        view.Refresh();
        auto openPreview = [&view]() {
            auto widget = view.Widgets().FindShared(PropertyEditorKind::Color);
            widget->OnDown(view, *Field(view, "color"), Pointer(ui::UIPointerAction::DOWN, 101.0f, 35.0f), {100.0f, 30.0f, 400.0f, 60.0f});
            view.OnPointerEvent(Pointer(ui::UIPointerAction::DOWN, 312.0f, 144.0f));
        };
        openPreview();
        ui::UIKeyEvent history;
        history.keyCode   = 0x5A;
        history.modifiers = kModCtrl;
        view.OnKeyEvent(history);
        EXPECT_FALSE(view.WantsPointerCapture());
        EXPECT_FLOAT_EQ(object.color.r, 0.2f);
        EXPECT_FLOAT_EQ(object.color.g, 0.4f);
        EXPECT_FLOAT_EQ(object.color.b, 0.6f);

        openPreview();
        history.keyCode = 0x59;
        view.OnKeyEvent(history);
        EXPECT_FALSE(view.WantsPointerCapture());
        EXPECT_FLOAT_EQ(object.color.r, edited.r);
        EXPECT_FLOAT_EQ(object.color.g, edited.g);
        EXPECT_FLOAT_EQ(object.color.b, edited.b);
    }

    TEST(ReflectedFormWidgetsTest, RegistryReplacementKeepsActiveSessionAliveUntilRelease)
    {
        ObjectType();
        ToggleObject      object;
        Services          services;
        ReflectedFormView view(nullptr, "", services.Context());
        view.Bind({&object, GetTypeNode(TypeInfo<ToggleObject>::RegisteredId())});
        view.SetHeaderVisible(false);
        view.Arrange({0.0f, 0.0f, 400.0f, 300.0f});
        auto                           stats   = std::make_shared<SessionStats>();
        auto                           widget  = std::make_shared<ReplaceableWidget>(stats);
        std::weak_ptr<ReflectedWidget> session = widget;
        view.Widgets().Register(PropertyEditorKind::Bool, std::move(widget));
        const float rowY = 4.0f + services.theme.metrics.sectionHeight + services.theme.metrics.rowHeight * 0.5f;
        EXPECT_EQ(view.OnPointerEvent(Pointer(ui::UIPointerAction::DOWN, 300.0f, rowY)), ui::UIEventResult::HANDLED);
        EXPECT_TRUE(view.WantsPointerCapture());
        view.Widgets().Register(PropertyEditorKind::Bool, std::make_shared<ReplaceableWidget>(std::make_shared<SessionStats>()));
        EXPECT_FALSE(session.expired());
        EXPECT_EQ(stats->releases, 0);
        EXPECT_TRUE(view.WantsPointerCapture());
        Paint(view);
        view.OnPointerEvent(Pointer(ui::UIPointerAction::UP, 300.0f, rowY));
        EXPECT_EQ(stats->ups, 1);
        EXPECT_EQ(stats->resets, 1);
        EXPECT_EQ(stats->releases, 1);
        EXPECT_TRUE(session.expired());
        EXPECT_FALSE(view.WantsPointerCapture());
    }

    TEST(ReflectedFormWidgetsTest, ColorWheelReusesRegistryResourceAndRecreatesAfterReset)
    {
        TextureRegistry  firstRegistry;
        TextureRegistry  secondRegistry;
        ui::UITextSystem firstText(nullptr, &firstRegistry);
        ui::UITextSystem secondText(nullptr, &secondRegistry);
        ColorPicker      first;
        ColorPicker      second;
        const auto       open = [](ColorPicker &picker, ui::UITextSystem &text) {
            picker.Open({100.0f, 30.0f, 400.0f, 60.0f}, {0.0f, 0.0f, 800.0f, 600.0f}, {}, {}, {}, &text);
        };
        open(first, firstText);
        first.Close();
        open(first, firstText);
        open(second, firstText);
        ASSERT_EQ(firstRegistry.images.size(), 1u);
        EXPECT_EQ(firstRegistry.creations, 1);
        EXPECT_EQ(firstRegistry.images.begin()->first, ColorPicker::WHEEL_TEXTURE_ID);
        {
            ColorPicker temporary;
            open(temporary, firstText);
        }
        // Closing one form cannot invalidate the wheel still used by others.
        EXPECT_EQ(firstRegistry.images.size(), 1u);
        EXPECT_EQ(firstRegistry.creations, 1);

        firstRegistry.images.clear();
        open(first, firstText);
        EXPECT_EQ(firstRegistry.images.size(), 1u);
        EXPECT_EQ(firstRegistry.creations, 2);
        open(first, secondText);
        EXPECT_EQ(secondRegistry.images.size(), 1u);
        EXPECT_EQ(secondRegistry.creations, 1);
        EXPECT_EQ(firstRegistry.images.size(), 1u);
    }

    TEST(ReflectedFormWidgetsTest, InlineCallbackOwnsReplacedWidgetUntilCommitOrCancel)
    {
        ObjectType();
        ToggleObject      object;
        Services          services;
        ReflectedFormView view(nullptr, "", services.Context());
        view.Bind({&object, GetTypeNode(TypeInfo<ToggleObject>::RegisteredId())});
        view.SetHeaderVisible(false);
        view.Arrange({0.0f, 0.0f, 400.0f, 300.0f});
        const float rowY             = 4.0f + services.theme.metrics.sectionHeight + services.theme.metrics.rowHeight * 0.5f;
        const auto  beginThenReplace = [&](const std::shared_ptr<SessionStats> &stats) {
            auto                           widget  = std::make_shared<InlineWidget>(stats);
            std::weak_ptr<ReflectedWidget> session = widget;
            view.Widgets().Register(PropertyEditorKind::Bool, std::move(widget));
            view.OnPointerEvent(Pointer(ui::UIPointerAction::DOWN, 300.0f, rowY));
            view.OnPointerEvent(Pointer(ui::UIPointerAction::UP, 300.0f, rowY));
            // The custom widget has only one kind. Other Generic registrations
            // cannot accidentally keep it alive after this replacement.
            view.Widgets().Register(PropertyEditorKind::Bool, std::make_shared<InlineWidget>(std::make_shared<SessionStats>()));
            return session;
        };
        auto committed = std::make_shared<SessionStats>();
        auto session   = beginThenReplace(committed);
        ASSERT_FALSE(session.expired());
        EXPECT_EQ(committed->releases, 0);
        ui::UIKeyEvent key;
        key.keyCode = 0x0D;
        view.OnKeyEvent(key);
        EXPECT_EQ(committed->commits, 1);
        EXPECT_EQ(committed->afterRefresh, 1);
        EXPECT_EQ(committed->releases, 1);
        EXPECT_TRUE(session.expired());

        auto canceled = std::make_shared<SessionStats>();
        session       = beginThenReplace(canceled);
        ASSERT_FALSE(session.expired());
        key.keyCode = 0x1B;
        view.OnKeyEvent(key);
        EXPECT_EQ(canceled->commits, 0);
        EXPECT_EQ(canceled->releases, 1);
        EXPECT_TRUE(session.expired());
    }

    TEST(ReflectedFormWidgetsTest, BeforeHistoryCanDestroyFormAndUnsubscribeSafely)
    {
        const auto                        *type = ObjectType();
        Services                           services;
        Object                             object;
        std::unique_ptr<ReflectedFormView> view;
        const auto                         destroy = services.commands.AddBeforeHistoryCallback([&]() { view.reset(); });
        view                                       = std::make_unique<ReflectedFormView>(nullptr, "", services.Context());
        view->Bind({&object, type});
        ASSERT_TRUE(view->Form().Edit(*Field(*view, "enabled"), Any(true), services.commands));
        EXPECT_TRUE(object.enabled);
        EXPECT_TRUE(services.commands.Undo());
        EXPECT_EQ(view, nullptr);
        EXPECT_FALSE(object.enabled);
        EXPECT_TRUE(services.commands.Redo());
        EXPECT_TRUE(object.enabled);
        services.commands.RemoveBeforeHistoryCallback(destroy);
    }

    TEST(ReflectedFormWidgetsTest, LocalHistoryKeysMayDestroyFormDuringPreparation)
    {
        const auto                        *type = ObjectType();
        Services                           services;
        Object                             object;
        std::unique_ptr<ReflectedFormView> view;
        const auto                         destroy  = services.commands.AddBeforeHistoryCallback([&]() { view.reset(); });
        const auto                         bindView = [&]() {
            view = std::make_unique<ReflectedFormView>(nullptr, "", services.Context());
            view->Bind({&object, type});
        };
        bindView();
        ASSERT_TRUE(view->Form().Edit(*Field(*view, "enabled"), Any(true), services.commands));

        ui::UIKeyEvent key;
        key.action    = ui::UIKeyAction::DOWN;
        key.keyCode   = 0x5A;
        key.modifiers = kModCtrl;
        EXPECT_EQ(view->OnKeyEvent(key), ui::UIEventResult::HANDLED);
        EXPECT_EQ(view, nullptr);
        EXPECT_FALSE(object.enabled);
        EXPECT_EQ(services.commands.GetUndoCount(), 0u);
        EXPECT_EQ(services.commands.GetRedoCount(), 1u);

        bindView();
        key.keyCode = 0x59;
        EXPECT_EQ(view->OnKeyEvent(key), ui::UIEventResult::HANDLED);
        EXPECT_EQ(view, nullptr);
        EXPECT_TRUE(object.enabled);
        EXPECT_EQ(services.commands.GetUndoCount(), 1u);
        EXPECT_EQ(services.commands.GetRedoCount(), 0u);
        services.commands.RemoveBeforeHistoryCallback(destroy);
    }

    TEST(ReflectedFormWidgetsTest, DirectEmptyHistoryCancelsLivePreviewWithoutDelayedRollback)
    {
        Object            object;
        Services          services;
        ReflectedFormView view(nullptr, "", services.Context());
        view.Bind({&object, ObjectType()});
        view.Arrange({0.0f, 0.0f, 800.0f, 600.0f});
        auto color = view.Widgets().FindShared(PropertyEditorKind::Color);
        ASSERT_NE(color, nullptr);
        ASSERT_TRUE(color->OnDown(view, *Field(view, "color"), Pointer(ui::UIPointerAction::DOWN, 101.0f, 35.0f), {100.0f, 30.0f, 400.0f, 60.0f}));
        view.OnPointerEvent(Pointer(ui::UIPointerAction::DOWN, 312.0f, 144.0f));
        ASSERT_TRUE(view.WantsPointerCapture());
        EXPECT_FALSE(services.commands.Undo());
        EXPECT_FALSE(view.WantsPointerCapture());
        EXPECT_FLOAT_EQ(object.color.r, 0.2f);
        EXPECT_FLOAT_EQ(object.color.g, 0.4f);
        EXPECT_FLOAT_EQ(object.color.b, 0.6f);
        view.OnPointerEvent(Pointer(ui::UIPointerAction::UP, 312.0f, 144.0f));
        EXPECT_EQ(services.commands.GetUndoCount(), 0u);
        EXPECT_EQ(services.commands.GetRedoCount(), 0u);
        EXPECT_FLOAT_EQ(object.color.r, 0.2f);
    }

    TEST(ReflectedFormWidgetsTest, InlineEditingHistoryUsesSharedBoundaryOnlyOnKeyDown)
    {
        Object            object;
        Services          services;
        ReflectedFormView view(nullptr, "", services.Context());
        view.Bind({&object, ObjectType()});
        ASSERT_TRUE(view.Form().Edit(*Field(view, "text"), Any(std::string("committed")), services.commands));
        int commits = 0;
        view.BeginTextEdit(*Field(view, "text"), -1, PropertyEditorKind::String, "preview", [&](const std::string &) {
            ++commits;
            return true;
        });
        ui::UIKeyEvent shiftedLetter;
        shiftedLetter.keyCode   = 0x5A;
        shiftedLetter.modifiers = kModShift;
        view.OnKeyEvent(shiftedLetter);
        EXPECT_EQ(services.commands.GetUndoCount(), 1u);
        EXPECT_EQ(object.text, "committed");
        ui::UIKeyEvent undo;
        undo.keyCode   = 0x5A;
        undo.modifiers = kModCtrl;
        EXPECT_EQ(view.OnKeyEvent(undo), ui::UIEventResult::HANDLED);
        EXPECT_EQ(object.text, "initial");
        EXPECT_EQ(commits, 0);
        EXPECT_EQ(services.commands.GetUndoCount(), 0u);
        EXPECT_EQ(services.commands.GetRedoCount(), 1u);
        undo.action = ui::UIKeyAction::UP;
        view.OnKeyEvent(undo);
        EXPECT_EQ(services.commands.GetRedoCount(), 1u);
        undo.action    = ui::UIKeyAction::DOWN;
        undo.modifiers = kModCtrl | kModShift;
        EXPECT_EQ(view.OnKeyEvent(undo), ui::UIEventResult::HANDLED);
        EXPECT_EQ(object.text, "committed");
        EXPECT_EQ(commits, 0);
    }

} // namespace sky::editor::widget_test
