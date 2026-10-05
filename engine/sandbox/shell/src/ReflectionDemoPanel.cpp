//
// Created on 2026/10/04.
//

#include <editor/shell/ReflectionDemoPanel.h>
#include <editor/shell/ReflectedFormView.h>
#include <editor/shell/UiDraw.h>

#include <editor/core/EditorCore.h>
#include <editor/core/resource/SandboxResources.h>

#include <core/math/Color.h>
#include <core/math/Transform.h>
#include <core/math/Vector2.h>
#include <core/math/Vector3.h>
#include <core/math/Vector4.h>
#include <core/type/TypeInfo.h>
#include <core/util/Uuid.h>
#include <framework/asset/DerivedDataCache.h>
#include <framework/serialization/PropertyCommon.h>
#include <framework/serialization/SerializationContext.h>
#include <ui/IUITextureRegistry.h>
#include <ui/text/UITextSystem.h>

#include <cmath>
#include <cstdint>
#include <fstream>
#include <iterator>
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

        const char *kDdcPlatform()
        {
#if defined(_WIN32)
            return "Win32";
#elif defined(__APPLE__)
            return "MacOS";
#else
            return "Linux";
#endif
        }

        bool ReadAllBytes(const std::string &path, std::vector<uint8_t> &out)
        {
            std::ifstream in(path, std::ios::binary);
            if (!in) {
                return false;
            }
            out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
            return !out.empty();
        }

        // Directly drawn save/floppy glyph, used when no SVG source is available
        // so the sample always has an icon (the "or use it directly" path).
        void MakeProceduralSaveIcon(uint32_t size, std::vector<uint8_t> &out)
        {
            out.assign(static_cast<size_t>(size) * size * 4, 0);
            const float s = static_cast<float>(size);
            auto inRect = [](float x, float y, float l, float t, float r, float b) {
                return x >= l && x < r && y >= t && y < b;
            };
            for (uint32_t y = 0; y < size; ++y) {
                for (uint32_t x = 0; x < size; ++x) {
                    const float fx = static_cast<float>(x) + 0.5f;
                    const float fy = static_cast<float>(y) + 0.5f;
                    if (!inRect(fx, fy, s * 0.12f, s * 0.12f, s * 0.88f, s * 0.88f)) {
                        continue;
                    }
                    uint8_t r = 0xD9, g = 0xD9, b = 0xDE;
                    if (inRect(fx, fy, s * 0.33f, s * 0.12f, s * 0.62f, s * 0.42f) ||
                        inRect(fx, fy, s * 0.25f, s * 0.54f, s * 0.75f, s * 0.88f)) {
                        r = g = b = 0x3A;
                    }
                    const size_t o = (static_cast<size_t>(y) * size + x) * 4;
                    out[o + 0] = r;
                    out[o + 1] = g;
                    out[o + 2] = b;
                    out[o + 3] = 0xFF;
                }
            }
        }

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
                const float iconSize = Theme().metrics.headerHeight - 10.0f;
                const float liveW = uc::TextWidth(live ? "Live: ON" : "Live: OFF", Theme().fonts.small, Text());
                return iconSize + 4.0f + liveW + 16.0f;
            }

            void PaintExtraHeader(sky::ui::UIPaintContext &context, const sky::ui::UIRect &rect) override
            {
                (void)rect;
                const UiTheme &th = Theme();
                EnsureSaveIcon();

                sky::ui::UIRect iconRect;
                sky::ui::UIRect liveRect;
                ExtraRects(iconRect, liveRect);

                // Save icon: baked from `resources/icons/save.svg` through the DDC.
                const bool   iconActive = saveFlash > 0;
                const uint32_t iconBg = iconActive ? th.colors.accentSoft
                                                   : (saveHover ? th.colors.rowHover : th.colors.fieldHover);
                const uint32_t iconBorder = (iconActive || saveHover) ? th.colors.accent : th.colors.border;
                uc::RoundedField(context, iconRect, iconBg, iconBorder, th.metrics.buttonRadius);
                if (saveIcon != sky::ui::UI_INVALID_TEXTURE) {
                    const sky::ui::UIRect dst{iconRect.left + 2.0f, iconRect.top + 2.0f,
                                              iconRect.right - 2.0f, iconRect.bottom - 2.0f};
                    context.AddTexturedQuad(dst, sky::ui::UIRect{0.0f, 0.0f, 1.0f, 1.0f}, saveIcon, uc::color::White);
                }

                const uint32_t liveBg = live ? th.colors.accentSoft
                                             : (liveHover ? th.colors.rowHover : th.colors.fieldHover);
                const uint32_t liveBorder = live ? th.colors.accentSoft
                                                 : (liveHover ? th.colors.accent : th.colors.border);
                uc::RoundedField(context, liveRect, liveBg, liveBorder, th.metrics.buttonRadius);
                uc::Text(context, live ? "Live: ON" : "Live: OFF", th.fonts.small, liveRect,
                         live ? th.colors.textOnAccent : th.colors.textMuted, Text(), uc::HAlign::Center);
            }

            sky::ui::UIEventResult HandleExtraHeaderPointer(const sky::ui::UIPointerEvent &event) override
            {
                sky::ui::UIRect iconRect;
                sky::ui::UIRect liveRect;
                ExtraRects(iconRect, liveRect);

                if (event.action == sky::ui::UIPointerAction::MOVE) {
                    const bool hoverIcon = iconRect.Contains(event.x, event.y);
                    const bool hoverLive = liveRect.Contains(event.x, event.y);
                    if (hoverIcon != saveHover || hoverLive != liveHover) {
                        saveHover = hoverIcon;
                        liveHover = hoverLive;
                        MarkPaintDirty();
                    }
                    return (hoverIcon || hoverLive) ? sky::ui::UIEventResult::HANDLED
                                                    : sky::ui::UIEventResult::UNHANDLED;
                }

                if (event.action != sky::ui::UIPointerAction::DOWN) {
                    return sky::ui::UIEventResult::UNHANDLED;
                }
                if (iconRect.Contains(event.x, event.y)) {
                    saveFlash = 24; // pressed feedback + a visible "save" response
                    MarkPaintDirty();
                    return sky::ui::UIEventResult::HANDLED;
                }
                if (liveRect.Contains(event.x, event.y)) {
                    live = !live;
                    liveFrame = 0;
                    MarkPaintDirty();
                    return sky::ui::UIEventResult::HANDLED;
                }
                return sky::ui::UIEventResult::UNHANDLED;
            }

            void OnPointerLeave(const sky::ui::UIPointerEvent &event) override
            {
                ReflectedFormView::OnPointerLeave(event);
                if (saveHover || liveHover) {
                    saveHover = false;
                    liveHover = false;
                    MarkPaintDirty();
                }
            }

            void OnViewTick() override
            {
                if (saveFlash > 0 && --saveFlash == 0) {
                    MarkPaintDirty();
                }
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
            // Splits the extra-header rect into the save-icon button and the Live
            // toggle, so paint and hit-testing agree on the layout.
            void ExtraRects(sky::ui::UIRect &iconRect, sky::ui::UIRect &liveRect) const
            {
                const sky::ui::UIRect r = HeaderExtraRect();
                const float iconSize = r.Height();
                iconRect = sky::ui::UIRect{r.left, r.top, r.left + iconSize, r.top + iconSize};
                liveRect = sky::ui::UIRect{iconRect.right + 4.0f, r.top, r.right, r.bottom};
            }

            // Produces the save icon once: bakes the resource SVG through the DDC
            // (cached below the sandbox resources dir), falling back to a directly
            // drawn glyph when the SVG is unavailable.
            void EnsureSaveIcon()
            {
                if (saveIconReady || Text() == nullptr) {
                    return;
                }
                sky::ui::IUITextureRegistry *registry = Text()->GetRegistry();
                if (registry == nullptr) {
                    return;
                }

                const uint32_t size = 18;
                std::vector<uint8_t> pixels;

                std::vector<uint8_t> source;
                const std::string path = SandboxResources::Resolve("icons/save.svg");
                if (ReadAllBytes(path, source)) {
                    const std::string settings = std::to_string(size) + "x" + std::to_string(size);
                    if (!sky::DerivedDataCache::Get().Fetch(source, "ui-icon-svg", settings, kDdcPlatform(), pixels)) {
                        pixels.clear();
                    }
                }
                if (pixels.empty()) {
                    MakeProceduralSaveIcon(size, pixels);
                }

                sky::ui::UIImageData image;
                image.width = size;
                image.height = size;
                image.pixels = std::move(pixels);
                saveIcon = registry->RegisterTexture(image);
                saveIconReady = true;
            }

            refldemo::DemoObject object;
            bool live = false;
            int  liveFrame = 0;
            int  liveTick = 0;

            sky::ui::UITextureId saveIcon = sky::ui::UI_INVALID_TEXTURE;
            bool                 saveIconReady = false;
            bool                 saveHover = false;
            bool                 liveHover = false;
            int                  saveFlash = 0;
        };

    } // namespace

    std::unique_ptr<sky::ui::UIElement> CreateReflectionDemoPanel(sky::ui::UITextSystem *text,
                                                                  const std::string &title)
    {
        return std::make_unique<ReflectionDemoPanel>(text, title);
    }

} // namespace sky::editor
