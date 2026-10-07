//
// Created on 2026/10/04.
//

#include <editor/shell/AssetReflectedWidget.h>
#include <editor/shell/UiDraw.h>

#include <editor/core/command/CommandService.h>

#include <core/util/Uuid.h>

#include <ui/UIPaintContext.h>
#include <ui/text/UITextSystem.h>

#include <algorithm>
#include <string>

namespace sky::editor {

    namespace {
        namespace uc = uidraw;
    }

    void AssetReflectedWidget::FormatLabel(const PropertyField &field, std::string &outLabel, bool &outValid)
    {
        outLabel          = "(none)";
        outValid          = true;
        Any         value = field.descriptor.GetValue();
        const Uuid *id    = value.GetAsConst<Uuid>();
        if (id == nullptr || !(*id)) {
            return;
        }
        outLabel                     = id->ToString();
        IEditorAssetCatalog *catalog = GetEditorAssetCatalog();
        if (catalog == nullptr) {
            return;
        }
        std::string name;
        if (catalog->GetName(*id, name)) {
            outLabel = name;
        }
        if (!field.control.assetType.empty()) {
            std::string type;
            if (catalog->GetType(*id, type)) {
                outValid = (type == field.control.assetType);
            }
        }
    }

    void AssetReflectedWidget::Paint(ReflectedWidgetHost &host, sky::ui::UIPaintContext &context, PropertyField &field, const sky::ui::UIRect &rect)
    {
        const UiTheme   &th = host.Skin().Theme();
        const UiMetrics &m  = th.metrics;
        std::string      label;
        bool             valid = true;
        FormatLabel(field, label, valid);
        const float           browseW = m.indentSmall + m.checkboxPad;
        const sky::ui::UIRect box{rect.left, rect.top, std::max(rect.left, rect.right - browseW - m.checkboxPad * 2.0f), rect.bottom};
        const sky::ui::UIRect browse{box.right + m.checkboxPad * 2.0f, rect.top, rect.right, rect.bottom};
        host.Skin().DrawField(context, box, false, !valid);
        uc::Text(context, label, th.fonts.value, sky::ui::UIRect{box.left + m.cellPadding, box.top, box.right - m.checkboxPad * 2.0f, box.bottom},
                 valid ? th.colors.text : th.colors.error, host.Text());
        host.Skin().DrawField(context, browse, host.IsHovered(field), false);
        uc::Text(context, "...", th.fonts.value, browse, th.colors.text, host.Text(), uc::HAlign::Center);
    }

    void AssetReflectedWidget::Open(PropertyField &field, const sky::ui::UIRect &control, const sky::ui::UIRect &bounds)
    {
        openField = &field;
        hoverItem = -1;
        items.clear();
        IEditorAssetCatalog *catalog = GetEditorAssetCatalog();
        if (catalog != nullptr) {
            items = catalog->Gather(field.control.assetType);
        }
        const UiMetrics &m     = GetDefaultUiTheme().metrics;
        const float      pad   = m.checkboxPad;
        const float      w     = std::max(control.Width() + m.padX * 8.0f, m.popupMinWidth);
        const int        count = static_cast<int>(items.size());
        const float      rowH  = m.popupItemHeight;
        const float      h     = std::min(m.popupMaxHeight, rowH * static_cast<float>(std::max(count, 1)) + pad * 3.0f);
        float            left  = control.left;
        float            top   = control.bottom + pad;
        if (top + h > bounds.bottom - pad * 2.0f) {
            top = control.top - h - pad;
        }
        left      = std::max(bounds.left + pad * 2.0f, std::min(left, bounds.right - w - pad * 2.0f));
        popupRect = sky::ui::UIRect{left, top, left + w, top + h};
        itemRects.clear();
        for (int i = 0; i < count; ++i) {
            const float iy = popupRect.top + pad * 1.5f + rowH * static_cast<float>(i);
            if (iy + rowH > popupRect.bottom - pad * 1.5f) {
                break;
            }
            itemRects.push_back(sky::ui::UIRect{popupRect.left + pad, iy, popupRect.right - pad, iy + rowH});
        }
    }

    bool
    AssetReflectedWidget::OnDown(ReflectedWidgetHost &host, PropertyField &field, const sky::ui::UIPointerEvent &event, const sky::ui::UIRect &rect)
    {
        Open(field, rect, host.ViewBounds());
        host.MarkDirty();
        return true;
    }

    void AssetReflectedWidget::Select(ReflectedWidgetHost &host, int index)
    {
        if (openField == nullptr || index < 0 || index >= static_cast<int>(items.size())) {
            return;
        }
        PropertyField *field = openField;
        const Uuid     id    = items[index].uuid;
        openField            = nullptr;
        itemRects.clear();
        items.clear();
        host.Form().Edit(*field, Any(id), host.Commands());
        host.RefreshForm();
    }

    void AssetReflectedWidget::PaintPopup(ReflectedWidgetHost &host, sky::ui::UIPaintContext &context)
    {
        if (openField == nullptr) {
            return;
        }
        const UiTheme &th = host.Skin().Theme();
        host.Skin().DrawPopup(context, popupRect);
        if (items.empty()) {
            uc::Text(context, "<no assets of type '" + openField->control.assetType + "'>", th.fonts.small,
                     sky::ui::UIRect{popupRect.left + 8.0f, popupRect.top + 3.0f, popupRect.right - 6.0f, popupRect.top + 23.0f}, th.colors.textMuted,
                     host.Text());
        }
        Any         value   = openField->descriptor.GetValue();
        const Uuid *current = value.GetAsConst<Uuid>();
        for (size_t i = 0; i < itemRects.size(); ++i) {
            const bool selected = current != nullptr && items[i].uuid == *current;
            host.Skin().DrawPopupItem(context, itemRects[i], static_cast<int>(i) == hoverItem, selected);
            const std::string &label = items[i].name.empty() ? items[i].path : items[i].name;
            uc::Text(context, label, th.fonts.small,
                     sky::ui::UIRect{itemRects[i].left + 8.0f, itemRects[i].top, itemRects[i].right - 6.0f, itemRects[i].bottom}, th.colors.text,
                     host.Text());
        }
    }

    bool AssetReflectedWidget::OnPopupPointer(ReflectedWidgetHost &host, const sky::ui::UIPointerEvent &event)
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

    bool AssetReflectedWidget::OnEscape(ReflectedWidgetHost &host)
    {
        (void)host;
        if (openField != nullptr) {
            openField = nullptr;
            return true;
        }
        return false;
    }

} // namespace sky::editor
