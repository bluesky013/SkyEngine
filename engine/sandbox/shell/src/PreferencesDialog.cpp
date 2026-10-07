//
// Created on 2026/10/06.
//

#include <editor/shell/PreferencesDialog.h>

#include <editor/core/input/KeyModifiers.h>
#include <editor/shell/UiDraw.h>
#include <editor/shell/UiTheme.h>
#include <ui/UIPaintContext.h>
#include <ui/text/UITextSystem.h>

#include <algorithm>
#include <cstdio>

namespace sky::editor {

    namespace uc = uidraw;

    namespace {
        const UiMetrics &M()
        {
            return GetDefaultUiTheme().metrics;
        }

        float Clamp01(float value)
        {
            return std::max(0.0f, std::min(1.0f, value));
        }

        std::string FormatNumber(double value)
        {
            char buffer[32];
            std::snprintf(buffer, sizeof(buffer), "%.2f", value);
            return buffer;
        }

        std::string FormatValue(const PreferenceValue &value)
        {
            switch (value.type) {
            case PreferenceType::BOOL: return value.boolValue ? "On" : "Off";
            case PreferenceType::INT: return std::to_string(value.intValue);
            case PreferenceType::FLOAT: return FormatNumber(value.floatValue);
            case PreferenceType::STRING: return value.stringValue;
            case PreferenceType::COLOR: {
                char buffer[16];
                std::snprintf(buffer, sizeof(buffer), "#%02X%02X%02X", static_cast<int>(Clamp01(value.colorValue.r) * 255.0f),
                              static_cast<int>(Clamp01(value.colorValue.g) * 255.0f), static_cast<int>(Clamp01(value.colorValue.b) * 255.0f));
                return buffer;
            }
            }
            return {};
        }

        bool IsSlider(const PreferenceEntry &entry)
        {
            return (entry.defaultValue.type == PreferenceType::FLOAT || entry.defaultValue.type == PreferenceType::INT) &&
                   entry.maxValue > entry.minValue;
        }

        uint32_t PackColor(const PreferenceColor &color)
        {
            const uint32_t r = static_cast<uint32_t>(Clamp01(color.r) * 255.0f);
            const uint32_t g = static_cast<uint32_t>(Clamp01(color.g) * 255.0f);
            const uint32_t b = static_cast<uint32_t>(Clamp01(color.b) * 255.0f);
            const uint32_t a = static_cast<uint32_t>(Clamp01(color.a) * 255.0f);
            return (a << 24) | (b << 16) | (g << 8) | r;
        }
    } // namespace

    PreferencesDialog::PreferencesDialog(sky::ui::UITextSystem *text) : textSystem(text), skin(GetDefaultUiTheme(), text)
    {
        SetVisible(false);
    }

    void PreferencesDialog::SetModel(PreferenceRegistry *inRegistry, PreferenceStore *inStore)
    {
        registry = inRegistry;
        store    = inStore;
    }

    int PreferencesDialog::GetPageCount() const
    {
        return registry != nullptr ? static_cast<int>(registry->GetPages().size()) : 0;
    }

    void PreferencesDialog::Open()
    {
        textKey.clear();
        dragKey.clear();
        hoverCategory = -1;
        hoverButton   = -1;
        ModalDialog::Open();
    }

    void PreferencesDialog::Close()
    {
        textKey.clear();
        dragKey.clear();
        ModalDialog::Close();
    }

    void PreferencesDialog::SetPageIndex(int index)
    {
        if (index < 0 || index >= GetPageCount()) {
            return;
        }
        pageIndex = index;
        textKey.clear();
        MarkPaintDirty();
    }

    sky::ui::UIRect PreferencesDialog::PanelRect() const
    {
        return CenteredPanel(M().preferencesPanelWidth, M().preferencesPanelHeight, 420.0f, 320.0f);
    }

    sky::ui::UIRect PreferencesDialog::ContentRect() const
    {
        const sky::ui::UIRect panel = PanelRect();
        return sky::ui::UIRect{panel.left, panel.top + M().titleBarHeight, panel.right, panel.bottom - M().footerHeight};
    }

    sky::ui::UIRect PreferencesDialog::CategoryRect(int index) const
    {
        const sky::ui::UIRect content = ContentRect();
        const float           top     = content.top + M().itemSpacing + static_cast<float>(index) * M().categoryRowHeight;
        return sky::ui::UIRect{content.left + M().itemSpacing, top, content.left + M().categoryWidth - M().itemSpacing,
                               top + M().categoryRowHeight - M().checkboxPad};
    }

    int PreferencesDialog::CategoryAt(float x, float y) const
    {
        for (int i = 0; i < GetPageCount(); ++i) {
            if (CategoryRect(i).Contains(x, y)) {
                return i;
            }
        }
        return -1;
    }

    sky::ui::UIRect PreferencesDialog::ButtonRect(int index) const
    {
        const sky::ui::UIRect panel     = PanelRect();
        const float           top       = panel.bottom - M().footerHeight + 11.0f;
        const float           rightEdge = panel.right - M().dialogMargin - static_cast<float>(index) * (M().buttonMinWidth + M().itemSpacing);
        return sky::ui::UIRect{rightEdge - M().buttonMinWidth, top, rightEdge, top + M().frameHeight};
    }

    int PreferencesDialog::ButtonAt(float x, float y) const
    {
        for (int i = 0; i < 4; ++i) {
            if (ButtonRect(i).Contains(x, y)) {
                return i;
            }
        }
        return -1;
    }

    void PreferencesDialog::BuildRows(std::vector<Row> &rows) const
    {
        rows.clear();
        if (registry == nullptr || pageIndex < 0 || pageIndex >= GetPageCount()) {
            return;
        }
        const sky::ui::UIRect content = ContentRect();
        const sky::ui::UIRect page{content.left + M().categoryWidth, content.top + M().itemSpacing, content.right - M().dialogMargin,
                                   content.bottom - M().itemSpacing};

        float y = page.top;
        for (const PreferenceSection &section : registry->GetPages()[pageIndex].sections) {
            y += M().sectionHeight;
            for (const PreferenceEntry &entry : section.entries) {
                const float height = entry.defaultValue.type == PreferenceType::COLOR ? M().colorRowHeight : M().preferenceRowHeight;
                Row         row;
                row.entry   = &entry;
                row.bounds  = sky::ui::UIRect{page.left, y, page.right, y + height};
                row.label   = sky::ui::UIRect{page.left, y, page.left + page.Width() * 0.45f, y + height};
                row.control = sky::ui::UIRect{page.left + page.Width() * 0.45f, y, page.right, y + height};
                rows.push_back(row);
                y += height + M().checkboxPad * 2.0f;
            }
        }
    }

    const PreferencesDialog::Row *PreferencesDialog::RowAt(float x, float y, std::vector<Row> &rows) const
    {
        for (const Row &row : rows) {
            if (row.bounds.Contains(x, y)) {
                return &row;
            }
        }
        return nullptr;
    }

    void PreferencesDialog::SetSlider(const Row &row, float x, int channel)
    {
        const PreferenceEntry &entry    = *row.entry;
        const sky::ui::UIRect  area     = row.control;
        const float            fraction = Clamp01(area.Width() > 0.0f ? (x - area.left) / area.Width() : 0.0f);

        if (entry.defaultValue.type == PreferenceType::COLOR) {
            PreferenceColor color;
            store->GetColor(entry.key, color);
            float *channels[4] = {&color.r, &color.g, &color.b, &color.a};
            *channels[channel] = fraction;
            store->SetColor(entry.key, color);
        } else if (entry.defaultValue.type == PreferenceType::INT) {
            const double value = entry.minValue + fraction * (entry.maxValue - entry.minValue);
            store->SetInt(entry.key, static_cast<int64_t>(value + 0.5));
        } else {
            const double value = entry.minValue + fraction * (entry.maxValue - entry.minValue);
            store->SetFloat(entry.key, value);
        }
        MarkPaintDirty();
    }

    void PreferencesDialog::CycleCombo(const Row &row)
    {
        const PreferenceEntry &entry = *row.entry;
        std::string            current;
        store->GetString(entry.key, current);
        const std::vector<std::string> &options = entry.options;
        if (options.empty()) {
            return;
        }
        std::size_t index = 0;
        for (std::size_t i = 0; i < options.size(); ++i) {
            if (options[i] == current) {
                index = i;
                break;
            }
        }
        index = (index + 1) % options.size();
        store->SetString(entry.key, options[index]);
        MarkPaintDirty();
    }

    void PreferencesDialog::FocusText(const Row &row, float x)
    {
        textKey = row.entry->key;
        std::string value;
        store->GetString(row.entry->key, value);
        textEdit.SetText(value);
        (void)x;
        MarkPaintDirty();
    }

    void PreferencesDialog::Activate(const Row &row, float x, float y)
    {
        const PreferenceEntry &entry = *row.entry;
        switch (entry.defaultValue.type) {
        case PreferenceType::BOOL: {
            bool value = false;
            store->GetBool(entry.key, value);
            store->SetBool(entry.key, !value);
            MarkPaintDirty();
            break;
        }
        case PreferenceType::INT:
        case PreferenceType::FLOAT:
            if (IsSlider(entry)) {
                SetSlider(row, x, -1);
            }
            break;
        case PreferenceType::STRING:
            if (!entry.options.empty()) {
                CycleCombo(row);
            } else if (row.control.Contains(x, y)) {
                FocusText(row, x);
            }
            break;
        case PreferenceType::COLOR: {
            const float bandH   = row.control.Height() / 4.0f;
            int         channel = static_cast<int>((y - row.control.top) / std::max(1.0f, bandH));
            channel             = std::max(0, std::min(3, channel));
            SetSlider(row, x, channel);
            break;
        }
        }
    }

    void PreferencesDialog::Commit()
    {
        if (store != nullptr) {
            store->Commit();
        }
        if (onApplied) {
            onApplied();
        }
    }

    void PreferencesDialog::Cancel()
    {
        if (store != nullptr) {
            store->Revert();
        }
        Close();
    }

    void PreferencesDialog::OnPaint(sky::ui::UIPaintContext &context)
    {
        if (!isOpen || registry == nullptr) {
            return;
        }

        const UiTheme        &th    = skin.Theme();
        const sky::ui::UIRect panel = PanelRect();

        PaintBackdrop(context);
        const sky::ui::UIRect body = skin.DrawPanel(context, panel, "Preferences", true);

        // Category list.
        const sky::ui::UIRect cats{body.left, body.top, body.left + M().categoryWidth, body.bottom - M().footerHeight + 11.0f};
        uc::Fill(context, cats, th.colors.section);
        const auto &pages = registry->GetPages();
        for (int i = 0; i < static_cast<int>(pages.size()); ++i) {
            const sky::ui::UIRect rect = CategoryRect(i);
            const RowState        state =
                (i == pageIndex) ? RowState::Selected : (i == hoverCategory ? RowState::Hover : (i % 2 ? RowState::Alt : RowState::Normal));
            skin.DrawRow(context, rect, state);
            uc::Text(context, pages[static_cast<std::size_t>(i)].title, th.fonts.label,
                     sky::ui::UIRect{rect.left + th.metrics.padX, rect.top, rect.right, rect.bottom}, th.colors.text, textSystem, uc::HAlign::Left,
                     uc::VAlign::Middle, true);
        }

        // Page rows.
        std::vector<Row> rows;
        BuildRows(rows);
        const sky::ui::UIRect page{body.left + M().categoryWidth, body.top + M().itemSpacing, body.right - M().dialogMargin,
                                   body.bottom - M().footerHeight};
        const auto           &sections = pages[static_cast<std::size_t>(pageIndex)].sections;

        std::size_t rowIndex = 0;
        float       y        = page.top;
        for (const PreferenceSection &section : sections) {
            skin.DrawSectionHeader(context, sky::ui::UIRect{page.left, y, page.right, y + M().sectionHeight}, section.title);
            y += M().sectionHeight;
            for (const PreferenceEntry &entry : section.entries) {
                if (rowIndex < rows.size()) {
                    const Row &row = rows[rowIndex];
                    skin.DrawRow(context, row.bounds, (rowIndex % 2) ? RowState::Alt : RowState::Normal);
                    uc::Text(context, entry.label, th.fonts.label, row.label, th.colors.text, textSystem, uc::HAlign::Left, uc::VAlign::Middle, true);

                    const PreferenceValue *value = store->FindWorking(entry.key);
                    if (value != nullptr) {
                        const sky::ui::UIRect control = row.control;
                        switch (entry.defaultValue.type) {
                        case PreferenceType::BOOL: {
                            const float           size = th.metrics.checkboxSize;
                            const sky::ui::UIRect box{control.left, (control.top + control.bottom - size) * 0.5f, control.left + size,
                                                      (control.top + control.bottom + size) * 0.5f};
                            skin.DrawCheckbox(context, box, value->boolValue, false);
                            break;
                        }
                        case PreferenceType::STRING:
                            if (!entry.options.empty()) {
                                skin.DrawField(context, control, false, false);
                                uc::Text(context, value->stringValue, th.fonts.value,
                                         sky::ui::UIRect{control.left + th.metrics.controlPad, control.top, control.right - M().indentSmall,
                                                         control.bottom},
                                         th.colors.text, textSystem, uc::HAlign::Left, uc::VAlign::Middle, true);
                                skin.DrawTriangle(context, control.right - M().cellPadding * 2.0f, (control.top + control.bottom) * 0.5f, true,
                                                  th.colors.textMuted);
                            } else {
                                skin.DrawField(context, control, entry.key == textKey, false);
                                const std::string shown = entry.key == textKey ? textEdit.GetText() : value->stringValue;
                                uc::Text(context, shown, th.fonts.value,
                                         sky::ui::UIRect{control.left + th.metrics.controlPad, control.top, control.right, control.bottom},
                                         th.colors.text, textSystem, uc::HAlign::Left, uc::VAlign::Middle, true);
                            }
                            break;
                        case PreferenceType::INT:
                        case PreferenceType::FLOAT:
                            if (IsSlider(entry)) {
                                const double current =
                                    entry.defaultValue.type == PreferenceType::INT ? static_cast<double>(value->intValue) : value->floatValue;
                                skin.DrawSlider(context, control, static_cast<float>(current), static_cast<float>(entry.minValue),
                                                static_cast<float>(entry.maxValue));
                                uc::Text(context, FormatValue(*value), th.fonts.value,
                                         sky::ui::UIRect{control.right - M().sliderValueWidth, control.top, control.right, control.bottom},
                                         th.colors.textMuted, textSystem, uc::HAlign::Right, uc::VAlign::Middle, false);
                            } else {
                                uc::Text(context, FormatValue(*value), th.fonts.value, control, th.colors.text, textSystem, uc::HAlign::Left,
                                         uc::VAlign::Middle, true);
                            }
                            break;
                        case PreferenceType::COLOR: {
                            const PreferenceColor color       = value->colorValue;
                            const float           bandH       = control.Height() / 4.0f;
                            const float           channels[4] = {color.r, color.g, color.b, color.a};
                            for (int c = 0; c < 4; ++c) {
                                const float           top = control.top + static_cast<float>(c) * bandH;
                                const sky::ui::UIRect band{control.left, top + 1.0f, control.right, top + bandH - 1.0f};
                                skin.DrawSlider(context, band, Clamp01(channels[c]), 0.0f, 1.0f);
                            }
                            break;
                        }
                        }
                    }
                }
                ++rowIndex;
                y += (entry.defaultValue.type == PreferenceType::COLOR ? M().colorRowHeight : M().preferenceRowHeight) + M().checkboxPad * 2.0f;
            }
        }

        // Footer buttons.
        const sky::ui::UIRect footer{panel.left, panel.bottom - M().footerHeight, panel.right, panel.bottom};
        uc::Fill(context, footer, th.colors.section);
        uc::HLine(context, panel.left, panel.right, footer.top, th.colors.border);
        const char *labels[4] = {"OK", "Cancel", "Apply", "Reset"};
        for (int i = 0; i < 4; ++i) {
            const sky::ui::UIRect btn     = ButtonRect(i);
            const bool            primary = (i == 0);
            const bool            hovered = (hoverButton == i);
            if (primary) {
                uc::RoundedField(context, btn, hovered ? th.colors.accentHover : th.colors.accent, th.colors.border, th.metrics.buttonRadius);
                uc::Text(context, labels[i], th.fonts.value, btn, th.colors.textOnAccent, textSystem, uc::HAlign::Center, uc::VAlign::Middle, false);
            } else {
                skin.DrawToolItem(context, btn, labels[i], hovered);
            }
        }
    }

    sky::ui::UIEventResult PreferencesDialog::OnPointerEvent(const sky::ui::UIPointerEvent &event)
    {
        if (!isOpen) {
            return sky::ui::UIEventResult::UNHANDLED;
        }

        if (event.action == sky::ui::UIPointerAction::MOVE) {
            hoverCategory = CategoryAt(event.x, event.y);
            hoverButton   = ButtonAt(event.x, event.y);
            if (!dragKey.empty()) {
                std::vector<Row> rows;
                BuildRows(rows);
                for (const Row &row : rows) {
                    if (row.entry->key == dragKey) {
                        SetSlider(row, event.x, dragChannel);
                        break;
                    }
                }
            }
            MarkPaintDirty();
            return sky::ui::UIEventResult::HANDLED;
        }

        if (event.action == sky::ui::UIPointerAction::UP) {
            dragKey.clear();
            dragChannel = -1;
            return sky::ui::UIEventResult::HANDLED;
        }

        if (event.action != sky::ui::UIPointerAction::DOWN) {
            return sky::ui::UIEventResult::HANDLED;
        }

        const int button = ButtonAt(event.x, event.y);
        if (button >= 0) {
            if (button == 0) {
                Commit();
                Close();
            } else if (button == 1) {
                Cancel();
            } else if (button == 2) {
                Commit();
            } else if (button == 3) {
                store->ResetPageToDefaults(registry->GetPages()[static_cast<std::size_t>(pageIndex)].id);
                MarkPaintDirty();
            }
            return sky::ui::UIEventResult::HANDLED;
        }

        const int category = CategoryAt(event.x, event.y);
        if (category >= 0) {
            SetPageIndex(category);
            return sky::ui::UIEventResult::HANDLED;
        }

        std::vector<Row> rows;
        BuildRows(rows);
        const Row *row = RowAt(event.x, event.y, rows);
        if (row != nullptr) {
            Activate(*row, event.x, event.y);
            const PreferenceType type = row->entry->defaultValue.type;
            if ((type == PreferenceType::INT || type == PreferenceType::FLOAT) && IsSlider(*row->entry)) {
                dragKey     = row->entry->key;
                dragChannel = -1;
            } else if (type == PreferenceType::COLOR) {
                dragKey           = row->entry->key;
                const float bandH = row->control.Height() / 4.0f;
                dragChannel       = std::max(0, std::min(3, static_cast<int>((event.y - row->control.top) / std::max(1.0f, bandH))));
            }
            return sky::ui::UIEventResult::HANDLED;
        }

        if (!textKey.empty()) {
            textKey.clear();
            MarkPaintDirty();
        }
        return sky::ui::UIEventResult::HANDLED;
    }

    sky::ui::UIEventResult PreferencesDialog::OnKeyEvent(const sky::ui::UIKeyEvent &event)
    {
        if (!isOpen || event.action == sky::ui::UIKeyAction::UP) {
            return sky::ui::UIEventResult::UNHANDLED;
        }

        constexpr uint32_t kEscape = 0x1B;
        constexpr uint32_t kReturn = 0x0D;

        if (event.keyCode == kEscape) {
            Cancel();
            return sky::ui::UIEventResult::HANDLED;
        }
        if (event.keyCode == kReturn) {
            if (!textKey.empty()) {
                textKey.clear();
                MarkPaintDirty();
            } else {
                Commit();
                Close();
            }
            return sky::ui::UIEventResult::HANDLED;
        }

        if (!textKey.empty()) {
            const bool shift = (event.modifiers & kModShift) != 0;
            const bool ctrl  = (event.modifiers & kModCtrl) != 0;
            if (textEdit.OnKey(event.keyCode, shift, ctrl)) {
                store->SetString(textKey, textEdit.GetText());
                MarkPaintDirty();
            }
        }
        return sky::ui::UIEventResult::HANDLED;
    }

    sky::ui::UIEventResult PreferencesDialog::OnTextInput(const sky::ui::UITextInputEvent &event)
    {
        if (!isOpen || textKey.empty()) {
            return sky::ui::UIEventResult::UNHANDLED;
        }
        if (textEdit.OnText(event.text)) {
            store->SetString(textKey, textEdit.GetText());
            MarkPaintDirty();
        }
        return sky::ui::UIEventResult::HANDLED;
    }

} // namespace sky::editor
