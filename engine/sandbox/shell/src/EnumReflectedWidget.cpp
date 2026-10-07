//
// Created on 2026/10/05.
//

#include <editor/shell/EnumReflectedWidget.h>
#include <editor/shell/UiDraw.h>

#include <editor/core/command/CommandService.h>
#include <editor/core/property/PropertyValidation.h>

#include <ui/UIPaintContext.h>
#include <ui/text/UITextSystem.h>

#include <algorithm>
#include <cstring>

namespace sky::editor {

    namespace {

        namespace uc = uidraw;

        int64_t EnumRaw(const Any &value)
        {
            int64_t           out  = 0;
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
            uint8_t      buffer[8] = {0};
            const size_t size      = std::min<size_t>(info->staticInfo->size, sizeof(buffer));
            std::memcpy(buffer, &raw, size);
            return Any::Create(info, buffer);
        }

    } // namespace

    void EnumReflectedWidget::Paint(ReflectedWidgetHost &host, sky::ui::UIPaintContext &context, PropertyField &field, const sky::ui::UIRect &rect)
    {
        const UiTheme   &th = host.Skin().Theme();
        const UiMetrics &m  = th.metrics;
        host.Skin().DrawField(context, rect, false, false);
        uc::Text(context, FormatPropertyValue(field.descriptor, field.control), th.fonts.value,
                 sky::ui::UIRect{rect.left + m.cellPadding, rect.top, rect.right - (m.indentSmall + m.checkboxPad), rect.bottom}, th.colors.text,
                 host.Text());
        if (!field.control.enumNames.empty()) {
            host.Skin().DrawTriangle(context, rect.right - (m.indentSmall - m.hairline), (rect.top + rect.bottom) * 0.5f, true, th.colors.textMuted);
        }
    }

    bool
    EnumReflectedWidget::OnDown(ReflectedWidgetHost &host, PropertyField &field, const sky::ui::UIPointerEvent &event, const sky::ui::UIRect &rect)
    {
        if (field.control.enumNames.empty()) {
            return false;
        }
        Open(field, rect, host.ViewBounds());
        host.MarkDirty();
        return true;
    }

    void EnumReflectedWidget::Open(PropertyField &field, const sky::ui::UIRect &control, const sky::ui::UIRect &bounds)
    {
        openField              = &field;
        hoverItem              = -1;
        const UiMetrics &m     = GetDefaultUiTheme().metrics;
        const float      pad   = m.checkboxPad;
        const float      rowH  = m.popupItemHeight;
        const int        count = static_cast<int>(field.control.enumNames.size());
        const float      h     = rowH * static_cast<float>(count) + pad * 2.0f;
        float            top   = control.bottom + pad;
        if (top + h > bounds.bottom - pad * 2.0f) {
            top = control.top - h - pad;
        }
        popupRect = sky::ui::UIRect{control.left, top, control.right, top + h};
        itemRects.clear();
        for (int i = 0; i < count; ++i) {
            itemRects.push_back(sky::ui::UIRect{popupRect.left + pad, popupRect.top + pad + rowH * static_cast<float>(i), popupRect.right - pad,
                                                popupRect.top + pad + rowH * static_cast<float>(i + 1)});
        }
    }

    int EnumReflectedWidget::CurrentIndex(const PropertyField &field) const
    {
        const int64_t raw = EnumRaw(field.descriptor.GetValue());
        for (size_t i = 0; i < field.control.enumValues.size(); ++i) {
            if (field.control.enumValues[i] == raw) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    void EnumReflectedWidget::Select(ReflectedWidgetHost &host, int index)
    {
        if (openField == nullptr || index < 0 || index >= static_cast<int>(openField->control.enumValues.size())) {
            return;
        }
        PropertyField *field = openField;
        const int64_t  value = field->control.enumValues[index];
        openField            = nullptr;
        itemRects.clear();
        host.Form().Edit(*field, MakeEnumValue(field->descriptor.GetType(), value), host.Commands());
        host.RefreshForm();
    }

    void EnumReflectedWidget::PaintPopup(ReflectedWidgetHost &host, sky::ui::UIPaintContext &context)
    {
        if (openField == nullptr) {
            return;
        }
        const UiTheme   &th = host.Skin().Theme();
        const UiMetrics &m  = th.metrics;
        host.Skin().DrawPopup(context, popupRect);
        const int current = CurrentIndex(*openField);
        for (size_t i = 0; i < itemRects.size(); ++i) {
            const bool selected = static_cast<int>(i) == current;
            host.Skin().DrawPopupItem(context, itemRects[i], static_cast<int>(i) == hoverItem, selected);
            if (selected) {
                const float cy = (itemRects[i].top + itemRects[i].bottom) * 0.5f;
                host.Skin().DrawCheck(context,
                                      sky::ui::UIRect{itemRects[i].left + m.checkboxPad * 2.0f, cy - m.cellPadding, itemRects[i].left + m.indentSmall,
                                                      cy + m.cellPadding},
                                      th.colors.textOnAccent);
            }
            uc::Text(context, openField->control.enumNames[i], th.fonts.value,
                     sky::ui::UIRect{itemRects[i].left + m.listLabelIndent, itemRects[i].top, itemRects[i].right - m.checkboxPad * 2.0f,
                                     itemRects[i].bottom},
                     th.colors.text, host.Text());
        }
    }

    bool EnumReflectedWidget::OnPopupPointer(ReflectedWidgetHost &host, const sky::ui::UIPointerEvent &event)
    {
        if (openField == nullptr) {
            return false;
        }
        if (event.action == sky::ui::UIPointerAction::MOVE) {
            int hover = -1;
            for (size_t i = 0; i < itemRects.size(); ++i) {
                if (itemRects[i].Contains(event.x, event.y)) {
                    hover = static_cast<int>(i);
                    break;
                }
            }
            if (hover != hoverItem) {
                hoverItem = hover;
                host.MarkDirty();
            }
            return true;
        }
        if (event.action == sky::ui::UIPointerAction::DOWN || event.action == sky::ui::UIPointerAction::UP) {
            for (size_t i = 0; i < itemRects.size(); ++i) {
                if (itemRects[i].Contains(event.x, event.y)) {
                    Select(host, static_cast<int>(i));
                    return true;
                }
            }
            openField = nullptr;
            itemRects.clear();
            host.MarkDirty();
            return true;
        }
        return true;
    }

    bool EnumReflectedWidget::OnEscape(ReflectedWidgetHost &host)
    {
        (void)host;
        if (openField != nullptr) {
            openField = nullptr;
            return true;
        }
        return false;
    }

} // namespace sky::editor
