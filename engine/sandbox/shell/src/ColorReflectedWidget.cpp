//
// Created on 2026/10/04.
//

#include <editor/shell/ColorReflectedWidget.h>
#include <editor/shell/UiDraw.h>

#include <editor/core/command/CommandService.h>

#include <ui/UIPaintContext.h>
#include <ui/text/UITextSystem.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <string>

namespace sky::editor {

    namespace {

        namespace uc = uidraw;

        uint32_t PackColor(const ColorRGBA &c)
        {
            const auto q = [](float v) -> uint32_t {
                return static_cast<uint32_t>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f);
            };
            return (q(c.a) << 24) | (q(c.b) << 16) | (q(c.g) << 8) | q(c.r);
        }

        const serialize::TypeMemberNode *FloatMemberAt(const TypeNode *node, int ordinal)
        {
            if (node == nullptr) {
                return nullptr;
            }
            int idx = 0;
            for (const auto &entry : node->members) {
                const TypeInfoRT *info = entry.second.info;
                if (info == nullptr || info->staticInfo == nullptr || !info->staticInfo->isFloatingPoint) {
                    continue;
                }
                if (idx == ordinal) {
                    return &entry.second;
                }
                ++idx;
            }
            return nullptr;
        }

        bool FloatMemberIndex(const PropertyDescriptor &descriptor, const char *name, int &outIndex)
        {
            const TypeNode *node = descriptor.GetStructType();
            if (node == nullptr) {
                return false;
            }
            int idx = 0;
            for (const auto &entry : node->members) {
                const TypeInfoRT *info = entry.second.info;
                if (info == nullptr || info->staticInfo == nullptr || !info->staticInfo->isFloatingPoint) {
                    continue;
                }
                std::string memberName(entry.first);
                for (char &ch : memberName) {
                    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
                }
                if (memberName == name) {
                    outIndex = idx;
                    return true;
                }
                ++idx;
            }
            return false;
        }

        float ReadChannel(const PropertyField &field, int ordinal)
        {
            Any copy = field.descriptor.GetValue();
            const serialize::TypeMemberNode *member = FloatMemberAt(field.descriptor.GetStructType(), ordinal);
            if (copy.Data() == nullptr || member == nullptr) {
                return 0.0f;
            }
            PropertyDescriptor child(copy.Data(), member, std::string(), field.descriptor.GetCategory());
            const Any value = child.GetValue();
            if (const float *f = value.GetAsConst<float>()) {
                return *f;
            }
            if (const double *d = value.GetAsConst<double>()) {
                return static_cast<float>(*d);
            }
            return 0.0f;
        }

        void ReadColor(const PropertyField &field, ColorRGBA &out)
        {
            int index = -1;
            if (FloatMemberIndex(field.descriptor, "r", index)) { out.r = ReadChannel(field, index); }
            if (FloatMemberIndex(field.descriptor, "g", index)) { out.g = ReadChannel(field, index); }
            if (FloatMemberIndex(field.descriptor, "b", index)) { out.b = ReadChannel(field, index); }
            if (FloatMemberIndex(field.descriptor, "a", index)) { out.a = ReadChannel(field, index); }
        }

        Any BuildColorValue(const PropertyField &field, const ColorRGBA &color)
        {
            Any copy = field.descriptor.GetValue();
            const TypeNode *node = field.descriptor.GetStructType();
            if (copy.Data() == nullptr || node == nullptr) {
                return {};
            }
            const char *names[4] = {"r", "g", "b", "a"};
            const float values[4] = {color.r, color.g, color.b, color.a};
            for (int c = 0; c < 4; ++c) {
                int target = -1;
                if (!FloatMemberIndex(field.descriptor, names[c], target)) {
                    continue;
                }
                const serialize::TypeMemberNode *member = FloatMemberAt(node, target);
                if (member == nullptr) {
                    continue;
                }
                PropertyDescriptor child(copy.Data(), member, std::string(), field.descriptor.GetCategory());
                child.SetValue(Any(values[c]));
            }
            return copy;
        }

    } // namespace

    sky::ui::UIRect ColorReflectedWidget::Segment(const sky::ui::UIRect &control, int index) const
    {
        const float left = control.left + 30.0f;
        const float span = std::max(1.0f, (control.right - left) / 4.0f);
        return sky::ui::UIRect{left + span * static_cast<float>(index), control.top,
                               left + span * static_cast<float>(index + 1), control.bottom};
    }

    sky::ui::UIRect ColorReflectedWidget::Slider(const sky::ui::UIRect &segment) const
    {
        return sky::ui::UIRect{segment.left + 12.0f, segment.top + 3.0f, segment.right - 2.0f, segment.bottom - 3.0f};
    }

    float ColorReflectedWidget::Fraction(float x, float left, float right) const
    {
        if (right <= left) {
            return 0.0f;
        }
        return std::clamp((x - left) / (right - left), 0.0f, 1.0f);
    }

    void ColorReflectedWidget::Paint(ReflectedWidgetHost &host, sky::ui::UIPaintContext &context, PropertyField &field,
                                     const sky::ui::UIRect &rect)
    {
        const UiTheme &th = host.Skin().Theme();
        ColorRGBA color;
        ReadColor(field, color);
        const float comp[4] = {color.r, color.g, color.b, color.a};

        const sky::ui::UIRect swatch{rect.left, rect.top + 1.0f, rect.left + 26.0f, rect.bottom - 1.0f};
        host.Skin().DrawColorSwatch(context, swatch, PackColor(color));

        static const char *labels[4] = {"R", "G", "B", "A"};
        for (int i = 0; i < 4; ++i) {
            const sky::ui::UIRect segment = Segment(rect, i);
            uc::Text(context, labels[i], th.fonts.tiny,
                     sky::ui::UIRect{segment.left, segment.top, segment.left + 12.0f, segment.bottom}, th.colors.textMuted, host.Text());
            const bool active = target == &field && dragComponent == i;
            const float v = active ? static_cast<float>(dragValue) : comp[i];
            const uint32_t tint = (i == 0) ? uc::RGB(0xE0, 0x40, 0x40)
                                  : (i == 1) ? uc::RGB(0x40, 0xC0, 0x40)
                                  : (i == 2) ? uc::RGB(0x40, 0x80, 0xE0)
                                             : uc::RGB(0x90, 0x90, 0x90);
            uc::Slider(context, Slider(segment), v, 0.0f, 1.0f, tint);
        }
    }

    bool ColorReflectedWidget::OnDown(ReflectedWidgetHost &host, PropertyField &field, const sky::ui::UIPointerEvent &event,
                                      const sky::ui::UIRect &rect)
    {
        target = &field;

        if (event.x < rect.left + 30.0f) {
            ColorRGBA current;
            ReadColor(field, current);
            ReflectedWidgetHost *h = &host;
            PropertyField *t = &field;
            picker.Open(rect, host.ViewBounds(), current,
                        [h, t](const ColorRGBA &col) {
                            t->descriptor.SetValue(BuildColorValue(*t, col));
                            h->MarkDirty();
                        },
                        [h, t](const ColorRGBA &col) {
                            h->Form().Edit(*t, BuildColorValue(*t, col), h->Commands());
                            h->RefreshForm();
                        },
                        host.Text());
            return true;
        }

        for (int i = 0; i < 4; ++i) {
            const sky::ui::UIRect segment = Segment(rect, i);
            if (event.x >= segment.left && event.x < segment.right) {
                dragControl = rect;
                dragComponent = i;
                const sky::ui::UIRect slider = Slider(segment);
                dragValue = Fraction(event.x, slider.left, slider.right);
                host.MarkDirty();
                return true;
            }
        }
        return false;
    }

    bool ColorReflectedWidget::OnMove(ReflectedWidgetHost &host, const sky::ui::UIPointerEvent &event)
    {
        if (dragComponent < 0) {
            return false;
        }
        const sky::ui::UIRect slider = Slider(Segment(dragControl, dragComponent));
        dragValue = Fraction(event.x, slider.left, slider.right);
        host.MarkDirty();
        return true;
    }

    bool ColorReflectedWidget::OnUp(ReflectedWidgetHost &host, const sky::ui::UIPointerEvent &event)
    {
        (void)event;
        if (dragComponent < 0) {
            return false;
        }
        const int component = dragComponent;
        const float value = std::clamp(static_cast<float>(dragValue), 0.0f, 1.0f);
        dragComponent = -1;
        if (target != nullptr) {
            ColorRGBA color;
            ReadColor(*target, color);
            float *comp[4] = {&color.r, &color.g, &color.b, &color.a};
            *comp[component] = value;
            host.Form().Edit(*target, BuildColorValue(*target, color), host.Commands());
            host.RefreshForm();
        }
        return true;
    }

    void ColorReflectedWidget::PaintPopup(ReflectedWidgetHost &host, sky::ui::UIPaintContext &context)
    {
        picker.Draw(context, host.Skin());
    }

    bool ColorReflectedWidget::OnPopupPointer(ReflectedWidgetHost &host, const sky::ui::UIPointerEvent &event)
    {
        const bool handled = picker.HandlePointer(event) == sky::ui::UIEventResult::HANDLED;
        host.MarkDirty();
        return handled;
    }

    bool ColorReflectedWidget::OnEscape(ReflectedWidgetHost &host)
    {
        if (picker.HandleEscape()) {
            host.MarkDirty();
            return true;
        }
        return false;
    }

} // namespace sky::editor
