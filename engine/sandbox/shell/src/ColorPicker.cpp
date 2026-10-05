//
// Created on 2026/10/04.
//

#include <editor/shell/ColorPicker.h>
#include <editor/shell/UiDraw.h>
#include <editor/shell/UiSkin.h>
#include <editor/shell/UiTheme.h>

#include <ui/UIPaintContext.h>
#include <ui/UIEvent.h>
#include <ui/text/UITextSystem.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

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

    } // namespace

    void ColorPicker::Open(const sky::ui::UIRect &anchor, const sky::ui::UIRect &bounds, const ColorRGBA &current,
                           LiveFn onLive, CommitFn onCommit, sky::ui::UITextSystem *text)
    {
        textSystem = text;
        live = std::move(onLive);
        commit = std::move(onCommit);
        oldColor = current;
        open = true;
        drag = Drag::None;

        const float w = 236.0f;
        const float h = 272.0f;
        float left = anchor.right - w;
        float top = anchor.bottom + 4.0f;
        if (top + h > bounds.bottom - 4.0f) {
            top = anchor.top - h - 4.0f;
        }
        left = std::max(bounds.left + 4.0f, std::min(left, bounds.right - w - 4.0f));
        popupRect = sky::ui::UIRect{left, top, left + w, top + h};
        discCx = left + w * 0.5f;
        discCy = top + 80.0f;
        discR = 60.0f;
        valueRect = sky::ui::UIRect{left + 18.0f, top + 148.0f, left + w - 18.0f, top + 163.0f};
        alphaRect = sky::ui::UIRect{left + 18.0f, top + 170.0f, left + w - 18.0f, top + 185.0f};
        hsvRect = sky::ui::UIRect{left + 14.0f, top + 192.0f, left + w - 14.0f, top + 208.0f};
        rgbaRect = sky::ui::UIRect{left + 14.0f, top + 212.0f, left + w - 14.0f, top + 228.0f};

        EnsureWheelTexture();
        uc::RgbToHsv(PackColor(current), workH, workS, workV, workA);
    }

    void ColorPicker::Close()
    {
        if (open && drag != Drag::None && live) {
            live(oldColor); // cancel in-flight preview
        }
        open = false;
        drag = Drag::None;
    }

    void ColorPicker::EnsureWheelTexture()
    {
        if (wheelReady || textSystem == nullptr) {
            return;
        }
        sky::ui::IUITextureRegistry *registry = textSystem->GetRegistry();
        if (registry == nullptr) {
            return;
        }
        const uint32_t size = 192;
        sky::ui::UIImageData image;
        image.width = size;
        image.height = size;
        image.pixels.resize(static_cast<size_t>(size) * size * 4, 0);
        const float c = (static_cast<float>(size) - 1.0f) * 0.5f;
        const float R = c;
        for (uint32_t y = 0; y < size; ++y) {
            for (uint32_t x = 0; x < size; ++x) {
                const float dx = static_cast<float>(x) - c;
                const float dy = static_cast<float>(y) - c;
                const float dist = std::sqrt(dx * dx + dy * dy);
                const size_t o = (static_cast<size_t>(y) * size + x) * 4;
                if (dist > R) {
                    continue;
                }
                float angle = std::atan2(dy, dx);
                if (angle < 0.0f) { angle += 6.2831853f; }
                const uint32_t argb = uc::HsvToRgb(angle / 6.2831853f, dist / R, 1.0f, 1.0f);
                image.pixels[o + 0] = static_cast<uint8_t>(argb & 0xFF);
                image.pixels[o + 1] = static_cast<uint8_t>((argb >> 8) & 0xFF);
                image.pixels[o + 2] = static_cast<uint8_t>((argb >> 16) & 0xFF);
                image.pixels[o + 3] = static_cast<uint8_t>(std::clamp(R - dist, 0.0f, 1.0f) * 255.0f);
            }
        }
        wheelTexture = registry->RegisterTexture(image);
        wheelReady = true;
    }

    void ColorPicker::ApplyPixel(float x, float y)
    {
        const float dx = x - discCx;
        const float dy = y - discCy;
        const float dist = std::sqrt(dx * dx + dy * dy);
        float angle = std::atan2(dy, dx);
        if (angle < 0.0f) { angle += 6.2831853f; }
        workH = angle / 6.2831853f;
        workS = std::clamp(dist / discR, 0.0f, 1.0f);
    }

    ColorRGBA ColorPicker::WorkingColor() const
    {
        const uint32_t argb = uc::HsvToRgb(workH, workS, workV, workA);
        ColorRGBA c;
        c.r = static_cast<float>(argb & 0xFF) / 255.0f;
        c.g = static_cast<float>((argb >> 8) & 0xFF) / 255.0f;
        c.b = static_cast<float>((argb >> 16) & 0xFF) / 255.0f;
        c.a = workA;
        return c;
    }

    float ColorPicker::ComponentFromX(float x, float left, float right) const
    {
        if (right <= left) {
            return 0.0f;
        }
        return std::clamp((x - left) / (right - left), 0.0f, 1.0f);
    }

    sky::ui::UIEventResult ColorPicker::HandlePointer(const sky::ui::UIPointerEvent &event)
    {
        if (event.action == sky::ui::UIPointerAction::MOVE) {
            if (drag == Drag::Disc) {
                ApplyPixel(event.x, event.y);
                if (live) { live(WorkingColor()); }
            } else if (drag == Drag::Value) {
                workV = ComponentFromX(event.x, valueRect.left, valueRect.right);
                if (live) { live(WorkingColor()); }
            } else if (drag == Drag::Alpha) {
                workA = ComponentFromX(event.x, alphaRect.left, alphaRect.right);
                if (live) { live(WorkingColor()); }
            }
            return sky::ui::UIEventResult::HANDLED;
        }

        if (event.action == sky::ui::UIPointerAction::DOWN) {
            const float dx = event.x - discCx;
            const float dy = event.y - discCy;
            const float dist = std::sqrt(dx * dx + dy * dy);
            if (dist <= discR) {
                drag = Drag::Disc;
                ApplyPixel(event.x, event.y);
            } else if (valueRect.Contains(event.x, event.y)) {
                drag = Drag::Value;
                workV = ComponentFromX(event.x, valueRect.left, valueRect.right);
            } else if (alphaRect.Contains(event.x, event.y)) {
                drag = Drag::Alpha;
                workA = ComponentFromX(event.x, alphaRect.left, alphaRect.right);
            } else {
                Close();
                return sky::ui::UIEventResult::HANDLED;
            }
            if (live) { live(WorkingColor()); }
            return sky::ui::UIEventResult::HANDLED;
        }

        if (event.action == sky::ui::UIPointerAction::UP) {
            if (drag != Drag::None) {
                drag = Drag::None;
                if (live) { live(oldColor); }      // restore so the commit captures the true old value
                if (commit) { commit(WorkingColor()); }
            }
            return sky::ui::UIEventResult::HANDLED;
        }
        return sky::ui::UIEventResult::HANDLED;
    }

    bool ColorPicker::HandleEscape()
    {
        if (!open) {
            return false;
        }
        Close();
        return true;
    }

    void ColorPicker::Draw(sky::ui::UIPaintContext &context, const UiSkin &skin)
    {
        if (!open) {
            return;
        }
        const UiTheme &th = skin.Theme();
        context.PushClip(skin.Theme().name.empty() ? popupRect : popupRect); // clip to popup area via caller bounds; kept simple
        skin.DrawPopup(context, popupRect);
        uc::Text(context, "Color", th.fonts.value,
                 sky::ui::UIRect{popupRect.left + 12.0f, popupRect.top + 4.0f, popupRect.right, popupRect.top + 22.0f},
                 th.colors.textMuted, textSystem);

        EnsureWheelTexture();
        const sky::ui::UIRect disc{discCx - discR, discCy - discR, discCx + discR, discCy + discR};
        if (wheelTexture != sky::ui::UI_INVALID_TEXTURE) {
            context.AddTexturedQuad(disc, sky::ui::UIRect{0.0f, 0.0f, 1.0f, 1.0f}, wheelTexture, 0xFFFFFFFF);
        }
        const float angle = workH * 6.2831853f;
        const float kx = discCx + std::cos(angle) * workS * discR;
        const float ky = discCy + std::sin(angle) * workS * discR;
        const uint32_t ring = uc::RGB(0xFF, 0xFF, 0xFF);
        context.AddRect(sky::ui::UIRect{kx - 5.0f, ky - 5.0f, kx + 5.0f, ky - 3.0f}, ring);
        context.AddRect(sky::ui::UIRect{kx - 5.0f, ky + 3.0f, kx + 5.0f, ky + 5.0f}, ring);
        context.AddRect(sky::ui::UIRect{kx - 5.0f, ky - 5.0f, kx - 3.0f, ky + 5.0f}, ring);
        context.AddRect(sky::ui::UIRect{kx + 3.0f, ky - 5.0f, kx + 5.0f, ky + 5.0f}, ring);

        const uint32_t hueColor = uc::HsvToRgb(workH, workS, 1.0f, 1.0f);
        const int vw = static_cast<int>(valueRect.Width());
        for (int k = 0; k < vw; ++k) {
            const float t = static_cast<float>(k) / static_cast<float>(std::max(vw, 1));
            context.AddRect(sky::ui::UIRect{valueRect.left + k, valueRect.top, valueRect.left + k + 1.0f, valueRect.bottom},
                            uc::LerpColor(0xFF000000u, hueColor, t));
        }
        const float vx = valueRect.left + workV * valueRect.Width();
        context.AddRect(sky::ui::UIRect{vx - 2.0f, valueRect.top - 1.0f, vx + 2.0f, valueRect.bottom + 1.0f}, ring);

        const ColorRGBA current = WorkingColor();
        uc::AlphaBar(context, alphaRect, uc::WithAlpha(PackColor(current), 1.0f), workA);

        const auto drawCells = [&](const sky::ui::UIRect &row, const char *const *labels,
                                   const std::string *values, int count) {
            const float cw = row.Width() / static_cast<float>(count);
            for (int i = 0; i < count; ++i) {
                const sky::ui::UIRect cell{row.left + cw * static_cast<float>(i), row.top,
                                           row.left + cw * static_cast<float>(i + 1) - 3.0f, row.bottom};
                skin.DrawField(context, cell, false, false);
                uc::Text(context, labels[i], th.fonts.tiny,
                         sky::ui::UIRect{cell.left + 4.0f, cell.top, cell.left + 15.0f, cell.bottom}, th.colors.textMuted, textSystem);
                uc::Text(context, values[i], th.fonts.small,
                         sky::ui::UIRect{cell.left + 15.0f, cell.top, cell.right - 3.0f, cell.bottom}, th.colors.text, textSystem);
            }
        };

        char hbuf[16] = {0}, sbuf[16] = {0}, vbuf[16] = {0};
        std::snprintf(hbuf, sizeof(hbuf), "%.0f", workH * 360.0f);
        std::snprintf(sbuf, sizeof(sbuf), "%.2f", workS);
        std::snprintf(vbuf, sizeof(vbuf), "%.2f", workV);
        const char *hsvLabels[3] = {"H", "S", "V"};
        const std::string hsvValues[3] = {hbuf, sbuf, vbuf};
        drawCells(hsvRect, hsvLabels, hsvValues, 3);

        char rbuf[8] = {0}, gbuf[8] = {0}, bbuf[8] = {0}, abuf[8] = {0};
        std::snprintf(rbuf, sizeof(rbuf), "%d", static_cast<int>(current.r * 255.0f + 0.5f));
        std::snprintf(gbuf, sizeof(gbuf), "%d", static_cast<int>(current.g * 255.0f + 0.5f));
        std::snprintf(bbuf, sizeof(bbuf), "%d", static_cast<int>(current.b * 255.0f + 0.5f));
        std::snprintf(abuf, sizeof(abuf), "%d", static_cast<int>(current.a * 255.0f + 0.5f));
        const char *rgbaLabels[4] = {"R", "G", "B", "A"};
        const std::string rgbaValues[4] = {rbuf, gbuf, bbuf, abuf};
        drawCells(rgbaRect, rgbaLabels, rgbaValues, 4);

        const sky::ui::UIRect preview{rgbaRect.left, rgbaRect.bottom + 8.0f, rgbaRect.left + 24.0f, rgbaRect.bottom + 32.0f};
        skin.DrawColorSwatch(context, preview, PackColor(current));
        char hex[16] = {0};
        std::snprintf(hex, sizeof(hex), "#%02X%02X%02X%02X",
                      static_cast<int>(current.r * 255.0f + 0.5f), static_cast<int>(current.g * 255.0f + 0.5f),
                      static_cast<int>(current.b * 255.0f + 0.5f), static_cast<int>(current.a * 255.0f + 0.5f));
        uc::Text(context, hex, th.fonts.value,
                 sky::ui::UIRect{preview.right + 8.0f, preview.top, alphaRect.right, preview.bottom}, th.colors.text, textSystem);
        context.PopClip();
    }

} // namespace sky::editor
