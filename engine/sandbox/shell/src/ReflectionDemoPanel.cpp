//
// Created on 2026/10/04.
//

#include <editor/shell/ReflectionDemoPanel.h>
#include <editor/shell/ReflectedFormView.h>
#include <editor/shell/UiDraw.h>

#include <editor/core/EditorCore.h>

#include <core/math/Color.h>
#include <core/math/Transform.h>
#include <core/math/Vector2.h>
#include <core/math/Vector3.h>
#include <core/math/Vector4.h>
#include <core/type/TypeInfo.h>
#include <core/util/Uuid.h>
#include <framework/serialization/PropertyCommon.h>
#include <framework/serialization/SerializationContext.h>

#include <cmath>
#include <cstdint>
#include <string>

namespace sky::editor::refldemo {

    enum class Quality {
        Low = 0,
        Medium = 1,
        High = 2
    };

    struct Placement {
        float x = 0.0f;
        float y = 0.0f;
    };

    struct DemoObject {
        bool        enabled = true;
        Quality     quality = Quality::Medium;
        int32_t     count = 10;
        float       exposure = 1.0f;
        std::string name = "Sky";
        Color       tint{0.9f, 0.5f, 0.2f, 1.0f};
        Vector3     direction{0.0f, 1.0f, 0.0f};
        Transform   transform;
        Uuid        texture;
        Placement   placement{1.0f, 2.0f};
        std::vector<float> weights{1.0f, 2.0f, 3.0f};
    };

    void EnsureRegistered()
    {
        static bool done = false;
        if (done) {
            return;
        }
        done = true;

        auto *context = SerializationContext::Get();
        context->Register<Quality>("ReflectionDemoQuality")
            .Enum(Quality::Low, "Low")
            .Enum(Quality::Medium, "Medium")
            .Enum(Quality::High, "High");

        context->Register<Placement>("ReflectionDemoPlacement")
            .Member<&Placement::x>("x")
            .Member<&Placement::y>("y");

        context->Register<DemoObject>("ReflectionDemoObject")
            .Member<&DemoObject::enabled>("enabled")
            .Member<&DemoObject::quality>("quality")
            .Member<&DemoObject::count>("count")
                .Property(static_cast<uint32_t>(CommonPropertyKey::RANGE_MIN), Any(0.0f))
                .Property(static_cast<uint32_t>(CommonPropertyKey::RANGE_MAX), Any(100.0f))
                .Property(static_cast<uint32_t>(CommonPropertyKey::RANGE_STEP), Any(5.0f))
            .Member<&DemoObject::exposure>("exposure")
                .Property(static_cast<uint32_t>(CommonPropertyKey::RANGE_MIN), Any(0.0f))
                .Property(static_cast<uint32_t>(CommonPropertyKey::RANGE_MAX), Any(2.0f))
                .Property(static_cast<uint32_t>(CommonPropertyKey::RANGE_STEP), Any(0.05f))
            .Member<&DemoObject::name>("name")
                .Property(static_cast<uint32_t>(CommonPropertyKey::LABEL), Any(std::string_view("Display Name")))
            .Member<&DemoObject::tint>("tint")
                .Property(static_cast<uint32_t>(CommonPropertyKey::EDITOR_KIND),
                          Any(static_cast<int32_t>(PropertyEditorKind::Color)))
            .Member<&DemoObject::direction>("direction")
                .Property(static_cast<uint32_t>(CommonPropertyKey::EDITOR_KIND),
                          Any(static_cast<int32_t>(PropertyEditorKind::Vector)))
            .Member<&DemoObject::transform>("transform")
            .Member<&DemoObject::texture>("texture")
                .Property(static_cast<uint32_t>(CommonPropertyKey::ASSET_TYPE), Any(std::string_view("Texture")))
            .Member<&DemoObject::placement>("placement")
            .Member<&DemoObject::weights>("weights");
    }

} // namespace sky::editor::refldemo

namespace sky::editor {

    namespace {

        namespace uc = uidraw;

        // Demo consumer of ReflectedFormView: adds a "Live" toggle that simulates
        // an external system writing the reflected object (to show data -> view).
        class ReflectionDemoPanel : public ReflectedFormView {
        public:
            ReflectionDemoPanel(sky::ui::UITextSystem *text, std::string title)
                : ReflectedFormView(text, std::move(title))
            {
                refldemo::EnsureRegistered();
                Bind(PropertyObject{&object, GetTypeNode(TypeInfo<refldemo::DemoObject>::RegisteredId())});
            }

            const char *GetTypeName() const override { return "ReflectionDemoPanel"; }

        protected:
            float ExtraHeaderWidth() const override
            {
                return uc::TextWidth(live ? "Live: ON" : "Live: OFF", Theme().fonts.small, Text()) + 16.0f;
            }

            void PaintExtraHeader(sky::ui::UIPaintContext &context, const sky::ui::UIRect &rect) override
            {
                const UiTheme &th = Theme();
                uc::RoundedField(context, rect, live ? th.colors.accentSoft : th.colors.fieldHover,
                                 live ? th.colors.accentSoft : th.colors.border, th.metrics.buttonRadius);
                uc::Text(context, live ? "Live: ON" : "Live: OFF", th.fonts.small, rect,
                         live ? th.colors.textOnAccent : th.colors.textMuted, Text(), uc::HAlign::Center);
            }

            sky::ui::UIEventResult HandleExtraHeaderPointer(const sky::ui::UIPointerEvent &event) override
            {
                if (event.action == sky::ui::UIPointerAction::DOWN && HeaderExtraRect().Contains(event.x, event.y)) {
                    live = !live;
                    liveFrame = 0;
                    MarkPaintDirty();
                    return sky::ui::UIEventResult::HANDLED;
                }
                return sky::ui::UIEventResult::UNHANDLED;
            }

            void OnViewTick() override
            {
                if (!live) {
                    return;
                }
                if (++liveFrame < 45) {
                    return;
                }
                liveFrame = 0;
                object.count = (object.count + 13) % 101;
                object.exposure = 0.5f + 0.5f * std::sin(static_cast<float>(liveTick) * 0.4f);
                object.quality = static_cast<refldemo::Quality>((static_cast<int>(object.quality) + 1) % 3);
                object.name = "tick " + std::to_string(liveTick);
                ++liveTick;
                EditorCore::GetPropertyChanges().Notify();
            }

        private:
            refldemo::DemoObject object;
            bool live = false;
            int  liveFrame = 0;
            int  liveTick = 0;
        };

    } // namespace

    std::unique_ptr<sky::ui::UIElement> CreateReflectionDemoPanel(sky::ui::UITextSystem *text,
                                                                  const std::string &title)
    {
        return std::make_unique<ReflectionDemoPanel>(text, title);
    }

} // namespace sky::editor
