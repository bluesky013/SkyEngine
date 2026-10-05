//
// Created on 2026/10/04.
//

#include <editor/shell/GenericReflectedWidget.h>
#include <editor/shell/UiDraw.h>

#include <editor/core/command/CommandService.h>
#include <editor/core/property/PropertyValidation.h>

#include <core/type/TypeInfo.h>

#include <ui/UIPaintContext.h>
#include <ui/text/UITextSystem.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace sky::editor {

    namespace {

        namespace uc = uidraw;

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

        int64_t EnumRaw(const Any &value)
        {
            int64_t out = 0;
            const TypeInfoRT *info = value.Info();
            if (info == nullptr || info->staticInfo == nullptr || value.Data() == nullptr) {
                return 0;
            }
            const size_t size = std::min<size_t>(info->staticInfo->size, sizeof(out));
            std::memcpy(&out, value.Data(), size);
            return out;
        }

        Any MakeEnumValue(const TypeInfoRT *info, int64_t raw)
        {
            if (info == nullptr || info->staticInfo == nullptr) {
                return {};
            }
            uint8_t buffer[8] = {0};
            const size_t size = std::min<size_t>(info->staticInfo->size, sizeof(buffer));
            std::memcpy(buffer, &raw, size);
            return Any::Create(info, buffer);
        }

    } // namespace

    sky::ui::UIRect GenericReflectedWidget::SliderRect(const sky::ui::UIRect &control) const
    {
        return sky::ui::UIRect{control.left, control.top, std::max(control.left, control.right - 60.0f - 6.0f), control.bottom};
    }

    sky::ui::UIRect GenericReflectedWidget::VectorCell(const sky::ui::UIRect &control, int index, int count) const
    {
        const float span = (control.right - control.left) / static_cast<float>(std::max(count, 1));
        return sky::ui::UIRect{control.left + span * static_cast<float>(index) + 1.0f, control.top,
                               control.left + span * static_cast<float>(index + 1) - 2.0f, control.bottom};
    }

    float GenericReflectedWidget::Fraction(float x, float left, float right) const
    {
        if (right <= left) {
            return 0.0f;
        }
        return std::clamp((x - left) / (right - left), 0.0f, 1.0f);
    }

    float GenericReflectedWidget::ReadComponent(const PropertyField &field, int index)
    {
        Any copy = field.descriptor.GetValue();
        const serialize::TypeMemberNode *member = FloatMemberAt(field.descriptor.GetStructType(), index);
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

    bool GenericReflectedWidget::WriteComponent(ReflectedWidgetHost &host, PropertyField &field, int index, float value)
    {
        Any copy = field.descriptor.GetValue();
        const serialize::TypeMemberNode *member = FloatMemberAt(field.descriptor.GetStructType(), index);
        if (copy.Data() == nullptr || member == nullptr) {
            return false;
        }
        PropertyDescriptor child(copy.Data(), member, std::string(), field.descriptor.GetCategory());
        child.SetValue(Any(value));
        return host.Form().Edit(field, Any(copy), host.Commands());
    }

    void GenericReflectedWidget::Paint(ReflectedWidgetHost &host, sky::ui::UIPaintContext &context,
                                       PropertyField &field, const sky::ui::UIRect &rect)
    {
        const UiTheme &th = host.Skin().Theme();

        if (field.kind == PropertyEditorKind::Bool) {
            const Any value = field.descriptor.GetValue();
            const bool on = value.GetAsConst<bool>() != nullptr && *value.GetAsConst<bool>();
            const float side = th.metrics.checkboxSize;
            const float cy = (rect.top + rect.bottom) * 0.5f;
            const sky::ui::UIRect box{rect.left, cy - side * 0.5f, rect.left + side, cy + side * 0.5f};
            host.Skin().DrawCheckbox(context, box, on, host.IsHovered(field));
            uc::Text(context, on ? "true" : "false", th.fonts.value,
                     sky::ui::UIRect{box.right + 8.0f, rect.top, rect.right, rect.bottom},
                     field.control.readOnly ? th.colors.textDisabled : th.colors.textMuted, host.Text());
            return;
        }

        if (field.kind == PropertyEditorKind::Vector) {
            const int count = std::max(1, static_cast<int>(field.control.componentCount));
            static const char *labels[4] = {"X", "Y", "Z", "W"};
            for (int i = 0; i < count && i < 4; ++i) {
                const sky::ui::UIRect cell = VectorCell(rect, i, count);
                host.Skin().DrawField(context, cell, false, false);
                uc::Text(context, labels[i], th.fonts.tiny,
                         sky::ui::UIRect{cell.left + 4.0f, cell.top, cell.left + 15.0f, cell.bottom}, th.colors.textMuted, host.Text());
                char buffer[32] = {0};
                std::snprintf(buffer, sizeof(buffer), "%.2f", ReadComponent(field, i));
                uc::Text(context, buffer, th.fonts.small,
                         sky::ui::UIRect{cell.left + 15.0f, cell.top, cell.right - 3.0f, cell.bottom}, th.colors.text, host.Text());
            }
            return;
        }

        if (field.control.hasRange && field.kind != PropertyEditorKind::String) {
            const sky::ui::UIRect slider = SliderRect(rect);
            double v = 0.0;
            const Any value = field.descriptor.GetValue();
            if (const float *f = value.GetAsConst<float>()) { v = *f; }
            else if (const int32_t *i = value.GetAsConst<int32_t>()) { v = *i; }
            if (dragField == &field) { v = dragValue; }
            host.Skin().DrawSlider(context, slider, static_cast<float>(v), static_cast<float>(field.control.rangeMin),
                                   static_cast<float>(field.control.rangeMax));
            const sky::ui::UIRect valueBox{slider.right + 6.0f, rect.top, rect.right, rect.bottom};
            char buffer[32] = {0};
            if (field.kind == PropertyEditorKind::Integer) {
                std::snprintf(buffer, sizeof(buffer), "%d", static_cast<int>(v));
            } else {
                std::snprintf(buffer, sizeof(buffer), "%.2f", v);
            }
            host.Skin().DrawField(context, valueBox, false, false);
            uc::Text(context, buffer, th.fonts.value, valueBox, th.colors.text, host.Text(), uc::HAlign::Center);
            return;
        }

        host.Skin().DrawField(context, rect, false, false);
        const float textRight = (field.kind == PropertyEditorKind::Enum) ? rect.right - 18.0f : rect.right - 5.0f;
        const uint32_t textColor = field.control.readOnly ? th.colors.textDisabled : th.colors.text;
        uc::Text(context, FormatPropertyValue(field.descriptor, field.control), th.fonts.value,
                 sky::ui::UIRect{rect.left + 6.0f, rect.top, textRight, rect.bottom}, textColor, host.Text());
        if (field.kind == PropertyEditorKind::Enum && !field.control.enumNames.empty()) {
            host.Skin().DrawTriangle(context, rect.right - 13.0f, (rect.top + rect.bottom) * 0.5f, true, th.colors.textMuted);
        }
    }

    void GenericReflectedWidget::BeginScalarTextEdit(ReflectedWidgetHost &host, PropertyField &field)
    {
        ReflectedWidgetHost *h = &host;
        PropertyField *t = &field;
        const PropertyEditorKind inputKind = (field.kind == PropertyEditorKind::String) ? PropertyEditorKind::String : field.kind;
        host.BeginTextEdit(field, -1, inputKind, FormatPropertyValue(field.descriptor, field.control),
                           [h, t](const std::string &text) {
                               bool ok = false;
                               Any value = ParseValueText(t->descriptor.GetType(), t->control, text, ok);
                               if (!ok) {
                                   return false;
                               }
                               h->Form().Edit(*t, std::move(value), h->Commands());
                               h->RefreshForm();
                               return true;
                           });
    }

    bool GenericReflectedWidget::OnDown(ReflectedWidgetHost &host, PropertyField &field,
                                        const sky::ui::UIPointerEvent &event, const sky::ui::UIRect &rect)
    {
        switch (field.kind) {
        case PropertyEditorKind::Bool: {
            const Any value = field.descriptor.GetValue();
            const bool on = value.GetAsConst<bool>() != nullptr && *value.GetAsConst<bool>();
            host.Form().Edit(field, Any(!on), host.Commands());
            host.RefreshForm();
            return true;
        }
        case PropertyEditorKind::Vector: {
            const int count = std::max(1, static_cast<int>(field.control.componentCount));
            for (int i = 0; i < count; ++i) {
                if (VectorCell(rect, i, count).Contains(event.x, rect.top)) {
                    ReflectedWidgetHost *h = &host;
                    PropertyField *t = &field;
                    const int component = i;
                    char initial[32] = {0};
                    std::snprintf(initial, sizeof(initial), "%.3f", ReadComponent(field, i));
                    host.BeginTextEdit(field, component, PropertyEditorKind::Float, initial,
                                       [this, h, t, component](const std::string &text) {
                                           char *end = nullptr;
                                           const float v = std::strtof(text.c_str(), &end);
                                           if (end == text.c_str()) {
                                               return false;
                                           }
                                           if (!WriteComponent(*h, *t, component, v)) {
                                               return false;
                                           }
                                           h->RefreshForm();
                                           return true;
                                       });
                    return true;
                }
            }
            return false;
        }
        default:
            if (field.control.hasRange && field.kind != PropertyEditorKind::String) {
                const sky::ui::UIRect slider = SliderRect(rect);
                dragField = &field;
                dragControl = rect;
                dragComponent = -1;
                dragValue = field.control.rangeMin +
                            Fraction(event.x, slider.left, slider.right) * (field.control.rangeMax - field.control.rangeMin);
                if (field.control.rangeStep > 0.0) {
                    const double step = field.control.rangeStep;
                    dragValue = field.control.rangeMin +
                                std::round((dragValue - field.control.rangeMin) / step) * step;
                }
                host.MarkDirty();
                return true;
            }
            BeginScalarTextEdit(host, field);
            return true;
        }
    }

    bool GenericReflectedWidget::OnMove(ReflectedWidgetHost &host, const sky::ui::UIPointerEvent &event)
    {
        if (dragField == nullptr || dragComponent >= 0) {
            return false;
        }
        const sky::ui::UIRect slider = SliderRect(dragControl);
        const float t = Fraction(event.x, slider.left, slider.right);
        dragValue = dragField->control.rangeMin + t * (dragField->control.rangeMax - dragField->control.rangeMin);
        if (dragField->control.rangeStep > 0.0) {
            const double step = dragField->control.rangeStep;
            dragValue = dragField->control.rangeMin + std::round((dragValue - dragField->control.rangeMin) / step) * step;
        }
        host.MarkDirty();
        return true;
    }

    bool GenericReflectedWidget::OnUp(ReflectedWidgetHost &host, const sky::ui::UIPointerEvent &event)
    {
        (void)event;
        if (dragField == nullptr || dragComponent >= 0) {
            return false;
        }
        PropertyField &field = *dragField;
        const double value = dragValue;
        dragField = nullptr;
        if (field.kind == PropertyEditorKind::Integer) {
            host.Form().Edit(field, Any(static_cast<int32_t>(value)), host.Commands());
        } else {
            host.Form().Edit(field, Any(static_cast<float>(value)), host.Commands());
        }
        host.RefreshForm();
        return true;
    }

    
    
    
    
} // namespace sky::editor
