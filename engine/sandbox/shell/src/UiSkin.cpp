//
// Created on 2026/10/04.
//

#include <editor/shell/UiSkin.h>
#include <editor/shell/UiDraw.h>

#include <ui/UIPaintContext.h>
#include <ui/text/UITextSystem.h>

#include <algorithm>

namespace sky::editor {

    namespace uc = uidraw;

    sky::ui::UIRect UiSkin::DrawPanel(sky::ui::UIPaintContext &context, const sky::ui::UIRect &bounds,
                                      const std::string &title, bool showTitle) const
    {
        const UiColors &c = theme->colors;
        const UiMetrics &m = theme->metrics;
        // Blender-like rounded panel with a subtle border.
        const float radius = m.panelRadius;
        uc::RoundedField(context, bounds, c.panel, c.borderSoft, radius);
        if (!showTitle) {
            return bounds; // the shell draws a tab header instead
        }
        // UE-like flat header with rounded top corners.
        const sky::ui::UIRect header{bounds.left, bounds.top, bounds.right, bounds.top + m.panelHeaderHeight};
        DrawPanelHeader(context, header);
        uc::Text(context, title, theme->fonts.title,
                 sky::ui::UIRect{header.left + m.padX, header.top, header.right - m.padX, header.bottom}, c.textMuted,
                 textSystem);
        return sky::ui::UIRect{bounds.left, header.bottom, bounds.right, bounds.bottom};
    }

    void UiSkin::DrawPanelHeader(sky::ui::UIPaintContext &context, const sky::ui::UIRect &header) const
    {
        const UiColors &c = theme->colors;
        const float radius = theme->metrics.panelRadius;
        uc::RoundedRect(context, sky::ui::UIRect{header.left, header.top, header.right, header.top + radius}, c.header,
                        radius);
        uc::Fill(context, sky::ui::UIRect{header.left, header.top + radius, header.right, header.bottom}, c.header);
        uc::HLine(context, header.left + 1.0f, header.right - 1.0f, header.bottom - 1.0f, c.borderSoft);
    }

    void UiSkin::DrawSectionHeader(sky::ui::UIPaintContext &context, const sky::ui::UIRect &rect,
                                   const std::string &title) const
    {
        const UiColors &c = theme->colors;
        const UiMetrics &m = theme->metrics;
        uc::RoundedGradient(context, rect, c.sectionTop, c.section, m.sectionRadius);
        uc::RoundedRect(context, sky::ui::UIRect{rect.left + 1.0f, rect.top + 3.0f, rect.left + 4.0f, rect.bottom - 3.0f},
                        c.accentSoft, 1.5f);
        uc::Text(context, title, theme->fonts.section,
                 sky::ui::UIRect{rect.left + 10.0f, rect.top, rect.right, rect.bottom}, c.text, textSystem);
    }

    void UiSkin::DrawRow(sky::ui::UIPaintContext &context, const sky::ui::UIRect &rect, RowState state) const
    {
        const UiColors &c = theme->colors;
        const UiMetrics &m = theme->metrics;
        uc::Fill(context, rect, state == RowState::Alt ? c.rowOdd : c.rowEven);
        if (state == RowState::Hover || state == RowState::Selected) {
            uc::RoundedRect(context,
                            sky::ui::UIRect{rect.left + 2.0f, rect.top + 1.0f, rect.right - 2.0f, rect.bottom - 1.0f},
                            state == RowState::Selected ? c.rowSelected : c.rowHover, m.rowRadius);
        }
    }

    void UiSkin::DrawField(sky::ui::UIPaintContext &context, const sky::ui::UIRect &rect, bool focused,
                           bool invalid) const
    {
        const UiColors &c = theme->colors;
        const uint32_t border = invalid ? c.error : (focused ? c.accent : c.border);
        uc::RoundedField(context, rect, focused ? c.fieldHover : c.field, border, theme->metrics.fieldRadius);
    }

    void UiSkin::DrawCheckbox(sky::ui::UIPaintContext &context, const sky::ui::UIRect &rect, bool checked,
                              bool hovered) const
    {
        const UiColors &c = theme->colors;
        if (checked) {
            uc::RoundedGradient(context, rect, c.accentSoft, c.checkOn, theme->metrics.checkboxRadius);
            DrawCheck(context, rect, c.textOnAccent);
        } else {
            uc::RoundedField(context, rect, hovered ? c.fieldHover : c.checkOff, hovered ? c.accent : c.border,
                             theme->metrics.checkboxRadius);
        }
    }

    void UiSkin::DrawSlider(sky::ui::UIPaintContext &context, const sky::ui::UIRect &rect, float value, float minValue,
                            float maxValue) const
    {
        const UiColors &c = theme->colors;
        const float cy = (rect.top + rect.bottom) * 0.5f;
        const float trackH = theme->metrics.sliderTrackHeight;
        const sky::ui::UIRect track{rect.left, cy - trackH * 0.5f, rect.right, cy + trackH * 0.5f};
        uc::RoundedRect(context, track, c.sliderTrack, trackH * 0.5f);

        const float t = maxValue > minValue ? std::clamp((value - minValue) / (maxValue - minValue), 0.0f, 1.0f) : 0.0f;
        const float hx = rect.left + (rect.right - rect.left) * t;
        if (hx > rect.left) {
            uc::RoundedRect(context, sky::ui::UIRect{rect.left, cy - trackH * 0.5f, hx, cy + trackH * 0.5f},
                            c.sliderFill, trackH * 0.5f);
        }
        const float knobW = theme->metrics.sliderKnobWidth;
        const float knobH = std::min(rect.Height() - 2.0f, 16.0f);
        uc::RoundedRect(context, sky::ui::UIRect{hx - knobW * 0.5f, cy - knobH * 0.5f, hx + knobW * 0.5f, cy + knobH * 0.5f},
                        c.sliderHandle, 5.0f);
    }

    void UiSkin::DrawColorSwatch(sky::ui::UIPaintContext &context, const sky::ui::UIRect &rect, uint32_t argb) const
    {
        uc::RoundedField(context, rect, argb, theme->colors.border, theme->metrics.swatchRadius);
    }

    void UiSkin::DrawScrollbar(sky::ui::UIPaintContext &context, const sky::ui::UIRect &track, float contentHeight,
                               float scrollTop) const
    {
        uc::ScrollBar(context, track, contentHeight, scrollTop, theme->colors.scrollBar);
    }

    void UiSkin::DrawPopup(sky::ui::UIPaintContext &context, const sky::ui::UIRect &rect) const
    {
        uc::SoftShadow(context, rect, theme->metrics.popupRadius);
        uc::RoundedField(context, rect, theme->colors.panel, theme->colors.border, theme->metrics.popupRadius);
    }

    void UiSkin::DrawPopupItem(sky::ui::UIPaintContext &context, const sky::ui::UIRect &rect, bool hovered,
                               bool selected) const
    {
        const UiColors &c = theme->colors;
        if (hovered) {
            uc::RoundedRect(context, rect, c.rowHover, 3.0f);
        } else if (selected) {
            uc::RoundedRect(context, rect, c.rowSelected, 3.0f);
        }
    }

    void UiSkin::DrawTab(sky::ui::UIPaintContext &context, const sky::ui::UIRect &rect, const std::string &title,
                         bool active, bool hovered) const
    {
        const UiColors &c = theme->colors;
        if (active) {
            uc::RoundedRect(context, sky::ui::UIRect{rect.left + 1.0f, rect.top + 2.0f, rect.right - 1.0f, rect.bottom},
                            c.tabActive, 5.0f);
            uc::RoundedRect(context,
                            sky::ui::UIRect{rect.left + 8.0f, rect.bottom - 3.0f, rect.right - 8.0f, rect.bottom - 1.0f},
                            c.accentSoft, 1.0f);
        } else if (hovered) {
            uc::RoundedRect(context, sky::ui::UIRect{rect.left + 1.0f, rect.top + 2.0f, rect.right - 1.0f, rect.bottom},
                            c.rowHover, 5.0f);
        } else {
            uc::Fill(context, rect, c.tabInactive);
        }
        uc::Text(context, title, theme->fonts.value,
                 sky::ui::UIRect{rect.left + 10.0f, rect.top, rect.right - 6.0f, rect.bottom},
                 (active || hovered) ? c.text : c.textMuted, textSystem);
    }

    void UiSkin::DrawToolItem(sky::ui::UIPaintContext &context, const sky::ui::UIRect &rect, const std::string &label,
                              bool hovered) const
    {
        if (hovered) {
            uc::RoundedRect(context, rect, theme->colors.rowHover, theme->metrics.buttonRadius);
        }
        uc::Text(context, label, theme->fonts.value,
                 sky::ui::UIRect{rect.left + 10.0f, rect.top, rect.right - 8.0f, rect.bottom}, theme->colors.text,
                 textSystem);
    }

    void UiSkin::DrawTriangle(sky::ui::UIPaintContext &context, float x, float cy, bool down, uint32_t color) const
    {
        const float s = 4.0f;
        if (down) {
            for (float i = 0.0f; i < s; ++i) {
                context.AddRect(sky::ui::UIRect{x - s + i, cy - s * 0.5f + i, x + s - i, cy - s * 0.5f + i + 1.0f}, color);
            }
        } else {
            for (float i = 0.0f; i < s; ++i) {
                context.AddRect(sky::ui::UIRect{x - s * 0.5f + i, cy - s + i * 2.0f, x - s * 0.5f + i + 1.0f, cy + s - i * 2.0f}, color);
            }
        }
    }

    void UiSkin::DrawCheck(sky::ui::UIPaintContext &context, const sky::ui::UIRect &box, uint32_t color) const
    {
        const float cx = (box.left + box.right) * 0.5f;
        const float cy = (box.top + box.bottom) * 0.5f;
        const auto segment = [&](float x0, float y0, float x1, float y1) {
            const int n = 6;
            for (int i = 0; i <= n; ++i) {
                const float t = static_cast<float>(i) / static_cast<float>(n);
                const float x = x0 + (x1 - x0) * t;
                const float y = y0 + (y1 - y0) * t;
                context.AddRect(sky::ui::UIRect{x - 1.0f, y - 1.0f, x + 1.0f, y + 1.0f}, color);
            }
        };
        segment(cx - 4.0f, cy - 0.5f, cx - 1.0f, cy + 3.0f);
        segment(cx - 1.0f, cy + 3.0f, cx + 5.0f, cy - 4.0f);
    }

} // namespace sky::editor
