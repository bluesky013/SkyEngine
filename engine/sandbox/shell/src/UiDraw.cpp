//
// Created on 2026/10/04.
//

#include <editor/shell/UiDraw.h>

#include <core/math/Color.h>
#include <editor/shell/UiTheme.h>
#include <ui/UIPaintContext.h>
#include <ui/text/UITextLayout.h>
#include <ui/text/UITextSystem.h>

#include <algorithm>
#include <cmath>
#include <functional>

namespace sky::editor::uidraw {

    namespace {

        uint32_t ScaleAlpha(uint32_t color, float a)
        {
            const uint32_t alpha  = (color >> 24) & 0xFFu;
            const uint32_t scaled = static_cast<uint32_t>(static_cast<float>(alpha) * std::clamp(a, 0.0f, 1.0f) + 0.5f);
            return (color & 0x00FFFFFFu) | ((scaled & 0xFFu) << 24);
        }

    } // namespace

    namespace {

    } // namespace

    float TextWidth(const std::string &text, uint32_t size, sky::ui::UITextSystem *textSystem)
    {
        if (textSystem == nullptr) {
            return 0.0f;
        }
        return sky::ui::UITextLayout::Measure(text, size, textSystem->GetAtlas()).width;
    }

    float TextHeight(const std::string &text, uint32_t size, sky::ui::UITextSystem *textSystem)
    {
        if (textSystem == nullptr) {
            return 0.0f;
        }
        return sky::ui::UITextLayout::Measure(text, size, textSystem->GetAtlas()).height;
    }

    std::string Ellipsize(const std::string &text, uint32_t size, float maxWidth, sky::ui::UITextSystem *textSystem)
    {
        if (textSystem == nullptr || TextWidth(text, size, textSystem) <= maxWidth) {
            return text;
        }
        const std::string suffix      = "...";
        const float       suffixWidth = TextWidth(suffix, size, textSystem);
        int               lo          = 0;
        int               hi          = static_cast<int>(text.size());
        while (lo < hi) {
            const int mid = (lo + hi + 1) / 2;
            if (TextWidth(text.substr(0, static_cast<size_t>(mid)), size, textSystem) + suffixWidth <= maxWidth) {
                lo = mid;
            } else {
                hi = mid - 1;
            }
        }
        return text.substr(0, static_cast<size_t>(lo)) + suffix;
    }

    void Fill(sky::ui::UIPaintContext &context, const sky::ui::UIRect &rect, uint32_t color)
    {
        context.AddRect(rect, color);
    }

    void Border(sky::ui::UIPaintContext &context, const sky::ui::UIRect &rect, uint32_t color, float thickness)
    {
        context.AddRect(sky::ui::UIRect{rect.left, rect.top, rect.right, rect.top + thickness}, color);
        context.AddRect(sky::ui::UIRect{rect.left, rect.bottom - thickness, rect.right, rect.bottom}, color);
        context.AddRect(sky::ui::UIRect{rect.left, rect.top, rect.left + thickness, rect.bottom}, color);
        context.AddRect(sky::ui::UIRect{rect.right - thickness, rect.top, rect.right, rect.bottom}, color);
    }

    void HLine(sky::ui::UIPaintContext &context, float x0, float x1, float y, uint32_t color)
    {
        context.AddRect(sky::ui::UIRect{x0, y, x1, y + 1.0f}, color);
    }

    uint32_t LerpColor(uint32_t top, uint32_t bottom, float t)
    {
        t              = std::clamp(t, 0.0f, 1.0f);
        const auto mix = [t, top, bottom](uint32_t shift) -> uint32_t {
            const int a = static_cast<int>((top >> shift) & 0xFF);
            const int b = static_cast<int>((bottom >> shift) & 0xFF);
            return static_cast<uint32_t>(a + static_cast<int>(static_cast<float>(b - a) * t)) & 0xFFu;
        };
        return (mix(24) << 24) | (mix(16) << 16) | (mix(8) << 8) | mix(0);
    }

    namespace {

        void
        RoundedRectImpl(sky::ui::UIPaintContext &context, const sky::ui::UIRect &rect, float radius, const std::function<uint32_t(float)> &colorAt)
        {
            const float h = rect.Height();
            const float w = rect.Width();
            if (h <= 0.0f || w <= 0.0f) {
                return;
            }
            radius           = std::min(radius, std::min(w, h) * 0.5f);
            const float rr   = radius * radius;
            const int   rows = static_cast<int>(std::ceil(h));
            for (int i = 0; i < rows; ++i) {
                const float y0    = rect.top + static_cast<float>(i);
                const float y1    = std::min(y0 + 1.0f, rect.bottom);
                const float yc    = (y0 + y1) * 0.5f;
                float       inset = 0.0f;
                if (radius > 0.0f) {
                    const float topCenter = rect.top + radius;
                    const float botCenter = rect.bottom - radius;
                    if (yc < topCenter) {
                        const float d = topCenter - yc;
                        inset         = radius - std::sqrt(std::max(0.0f, rr - d * d));
                    } else if (yc > botCenter) {
                        const float d = yc - botCenter;
                        inset         = radius - std::sqrt(std::max(0.0f, rr - d * d));
                    }
                }
                const float    t     = (yc - rect.top) / h;
                const uint32_t color = colorAt(t);
                if (inset <= 0.0f) {
                    // Straight-edge row: keep it crisp, no feathering.
                    context.AddRect(sky::ui::UIRect{rect.left, y0, rect.right, y1}, color);
                } else {
                    // Rounded corner row: anti-alias the curved edge with coverage.
                    const float xL = rect.left + inset;
                    const float xR = rect.right - inset;
                    const float fL = std::ceil(xL);
                    const float fR = std::floor(xR);
                    if (fR > fL) {
                        context.AddRect(sky::ui::UIRect{fL, y0, fR, y1}, color);
                    }
                    const float aL = fL - xL;
                    if (aL > 0.003f) {
                        context.AddRect(sky::ui::UIRect{std::floor(xL), y0, fL, y1}, ScaleAlpha(color, aL));
                    }
                    const float aR = xR - fR;
                    if (aR > 0.003f) {
                        context.AddRect(sky::ui::UIRect{fR, y0, std::ceil(xR), y1}, ScaleAlpha(color, aR));
                    }
                }
            }
        }

    } // namespace

    void RoundedRect(sky::ui::UIPaintContext &context, const sky::ui::UIRect &rect, uint32_t color, float radius)
    {
        context.AddRoundedRect(rect, color, radius);
    }

    void RoundedGradient(sky::ui::UIPaintContext &context, const sky::ui::UIRect &rect, uint32_t topColor, uint32_t bottomColor, float radius)
    {
        RoundedRectImpl(context, rect, radius, [topColor, bottomColor](float t) { return LerpColor(topColor, bottomColor, t); });
    }

    void RoundedField(sky::ui::UIPaintContext &context, const sky::ui::UIRect &rect, uint32_t bg, uint32_t border, float radius)
    {
        RoundedRect(context, rect, border, radius);
        const sky::ui::UIRect inner{rect.left + 1.0f, rect.top + 1.0f, rect.right - 1.0f, rect.bottom - 1.0f};
        RoundedRect(context, inner, bg, std::max(0.0f, radius - 1.0f));
    }

    void SoftShadow(sky::ui::UIPaintContext &context, const sky::ui::UIRect &rect, float radius)
    {
        constexpr uint32_t kShadow = 0x50000000; // translucent black (ABGR)
        for (int i = 3; i >= 1; --i) {
            const sky::ui::UIRect r{rect.left - static_cast<float>(i), rect.top + static_cast<float>(i) * 0.5f, rect.right + static_cast<float>(i),
                                    rect.bottom + static_cast<float>(i)};
            RoundedRect(context, r, kShadow, radius + static_cast<float>(i));
        }
    }

    void Field(sky::ui::UIPaintContext &context, const sky::ui::UIRect &rect, uint32_t bg, uint32_t border)
    {
        RoundedField(context, rect, bg, border, 4.0f * GetThemeScale());
    }

    void Slider(sky::ui::UIPaintContext &context, const sky::ui::UIRect &rect, float value, float minV, float maxV, uint32_t fillColor)
    {
        const float           s      = GetThemeScale();
        const float           cy     = (rect.top + rect.bottom) * 0.5f;
        const float           trackH = 6.0f * s;
        const sky::ui::UIRect track{rect.left, cy - trackH * 0.5f, rect.right, cy + trackH * 0.5f};
        RoundedRect(context, track, color::Field, trackH * 0.5f);

        const float t  = maxV > minV ? std::clamp((value - minV) / (maxV - minV), 0.0f, 1.0f) : 0.0f;
        const float hx = rect.left + (rect.right - rect.left) * t;
        if (hx > rect.left) {
            RoundedRect(context, sky::ui::UIRect{rect.left, cy - trackH * 0.5f, hx, cy + trackH * 0.5f}, fillColor, trackH * 0.5f);
        }
        const float knobH = std::min(rect.Height() - 2.0f * s, 16.0f * s);
        RoundedRect(context, sky::ui::UIRect{hx - 6.0f * s, cy - knobH * 0.5f, hx + 6.0f * s, cy + knobH * 0.5f}, color::SliderHandle, 5.0f * s);
    }

    void Text(sky::ui::UIPaintContext &context,
              const std::string       &inText,
              uint32_t                 size,
              const sky::ui::UIRect   &rect,
              uint32_t                 textColor,
              sky::ui::UITextSystem   *textSystem,
              HAlign                   hAlign,
              VAlign                   vAlign,
              bool                     ellipsize)
    {
        if (textSystem == nullptr || rect.IsEmpty() || inText.empty()) {
            return;
        }
        const std::string           glyphs = ellipsize ? Ellipsize(inText, size, std::max(0.0f, rect.Width() - 2.0f), textSystem) : inText;
        const sky::ui::UITextExtent extent = sky::ui::UITextLayout::Measure(glyphs, size, textSystem->GetAtlas());

        float x = rect.left;
        if (hAlign == HAlign::Center) {
            x = rect.left + (rect.Width() - extent.width) * 0.5f;
        } else if (hAlign == HAlign::Right) {
            x = rect.right - extent.width;
        }

        float y = rect.top;
        if (vAlign == VAlign::Middle) {
            sky::ui::IUIFontProvider *provider = textSystem->GetProvider();
            if (provider != nullptr) {
                // Center by the text ink (ascender/descender), not the full line
                // box, so glyphs sit optically centered in tight fields.
                const sky::ui::UIFontMetrics metrics = provider->GetFontMetrics(size);
                y = rect.top + rect.Height() * 0.5f + (metrics.ascender - metrics.descender) * 0.5f - metrics.lineHeight;
            } else {
                y = rect.top + (rect.Height() - extent.height) * 0.5f;
            }
        } else if (vAlign == VAlign::Bottom) {
            y = rect.bottom - extent.height;
        }

        // Clip horizontally (for the ellipsis) but give a little vertical slack so
        // glyphs are never cut top/bottom by a tight control box.
        sky::ui::UIRect clip = rect;
        clip.top -= 6.0f;
        clip.bottom += 6.0f;
        context.PushClip(clip);
        sky::ui::UITextLayout::Emit(context, glyphs, size, x, y, textColor, textSystem->GetAtlas());
        context.PopClip();
    }

    uint32_t WithAlpha(uint32_t argb, float a)
    {
        const uint32_t ab = static_cast<uint32_t>(std::clamp(a, 0.0f, 1.0f) * 255.0f + 0.5f) & 0xFFu;
        return (ab << 24) | (argb & 0x00FFFFFFu);
    }

    uint32_t HsvToRgb(float h, float s, float v, float a)
    {
        const Color c = FromHSV(h, s, v, a);
        const auto  q = [](float comp) { return static_cast<uint32_t>(std::clamp(comp, 0.0f, 1.0f) * 255.0f + 0.5f); };
        return (q(c.a) << 24) | (q(c.b) << 16) | (q(c.g) << 8) | q(c.r);
    }

    void RgbToHsv(uint32_t argb, float &h, float &s, float &v, float &a)
    {
        const Color    c{static_cast<float>((argb >> 0) & 0xFF) / 255.0f, static_cast<float>((argb >> 8) & 0xFF) / 255.0f,
                      static_cast<float>((argb >> 16) & 0xFF) / 255.0f, static_cast<float>((argb >> 24) & 0xFF) / 255.0f};
        const ColorHSV hsv = ToHSV(c);
        h                  = hsv.h;
        s                  = hsv.s;
        v                  = hsv.v;
        a                  = c.a;
    }

    void Checker(sky::ui::UIPaintContext &context, const sky::ui::UIRect &rect, float cell)
    {
        const uint32_t light = RGB(0x88, 0x88, 0x88);
        const uint32_t dark  = RGB(0x55, 0x55, 0x55);
        int            row   = 0;
        for (float y = rect.top; y < rect.bottom; y += cell, ++row) {
            int col = 0;
            for (float x = rect.left; x < rect.right; x += cell, ++col) {
                const sky::ui::UIRect c{x, y, std::min(x + cell, rect.right), std::min(y + cell, rect.bottom)};
                Fill(context, c, ((row + col) % 2 == 0) ? light : dark);
            }
        }
    }

    void HueRing(sky::ui::UIPaintContext &context, float cx, float cy, float outerR, float innerR, float value, float markerHue)
    {
        const int   segments = std::max(24, static_cast<int>(outerR * 1.4f));
        const float step     = 2.0f;
        for (int i = 0; i < segments; ++i) {
            const float    t    = static_cast<float>(i) / static_cast<float>(segments);
            const float    ang  = t * 6.2831853f;
            const uint32_t col  = HsvToRgb(t, 1.0f, std::max(0.08f, value));
            const float    cosA = std::cos(ang);
            const float    sinA = std::sin(ang);
            for (float r = innerR; r <= outerR; r += step) {
                const float x = cx + cosA * r;
                const float y = cy + sinA * r;
                context.AddRect(sky::ui::UIRect{x - step * 0.5f, y - step * 0.5f, x + step * 0.5f, y + step * 0.5f}, col);
            }
        }
        const float    mang = markerHue * 6.2831853f;
        const float    mr   = (innerR + outerR) * 0.5f;
        const float    mx   = cx + std::cos(mang) * mr;
        const float    my   = cy + std::sin(mang) * mr;
        const uint32_t ring = RGB(0xFF, 0xFF, 0xFF);
        context.AddRect(sky::ui::UIRect{mx - 4.0f, my - 4.0f, mx + 4.0f, my - 2.0f}, ring);
        context.AddRect(sky::ui::UIRect{mx - 4.0f, my + 2.0f, mx + 4.0f, my + 4.0f}, ring);
        context.AddRect(sky::ui::UIRect{mx - 4.0f, my - 4.0f, mx - 2.0f, my + 4.0f}, ring);
        context.AddRect(sky::ui::UIRect{mx + 2.0f, my - 4.0f, mx + 4.0f, my + 4.0f}, ring);
    }

    void ColorSquare(sky::ui::UIPaintContext &context, const sky::ui::UIRect &rect, float hue, float sat, float val)
    {
        Fill(context, rect, HsvToRgb(hue, 1.0f, 1.0f));
        const int w = static_cast<int>(rect.Width());
        for (int k = 0; k < w; ++k) {
            const float t = static_cast<float>(k) / static_cast<float>(std::max(w, 1));
            context.AddRect(sky::ui::UIRect{rect.left + k, rect.top, rect.left + k + 1.0f, rect.bottom}, WithAlpha(0xFFFFFFFF, 1.0f - t));
        }
        const int h = static_cast<int>(rect.Height());
        for (int k = 0; k < h; ++k) {
            const float t = static_cast<float>(k) / static_cast<float>(std::max(h, 1));
            context.AddRect(sky::ui::UIRect{rect.left, rect.top + k, rect.right, rect.top + k + 1.0f}, WithAlpha(0xFF000000, t));
        }
        const float    kx   = rect.left + std::clamp(sat, 0.0f, 1.0f) * rect.Width();
        const float    ky   = rect.top + (1.0f - std::clamp(val, 0.0f, 1.0f)) * rect.Height();
        const uint32_t ring = RGB(0xFF, 0xFF, 0xFF);
        context.AddRect(sky::ui::UIRect{kx - 5.0f, ky - 5.0f, kx + 5.0f, ky - 3.0f}, ring);
        context.AddRect(sky::ui::UIRect{kx - 5.0f, ky + 3.0f, kx + 5.0f, ky + 5.0f}, ring);
        context.AddRect(sky::ui::UIRect{kx - 5.0f, ky - 5.0f, kx - 3.0f, ky + 5.0f}, ring);
        context.AddRect(sky::ui::UIRect{kx + 3.0f, ky - 5.0f, kx + 5.0f, ky + 5.0f}, ring);
    }

    void AlphaBar(sky::ui::UIPaintContext &context, const sky::ui::UIRect &rect, uint32_t rgb, float alpha)
    {
        Checker(context, rect, 7.0f);
        const int w = static_cast<int>(rect.Width());
        for (int k = 0; k < w; ++k) {
            const float t = static_cast<float>(k) / static_cast<float>(std::max(w, 1));
            context.AddRect(sky::ui::UIRect{rect.left + k, rect.top, rect.left + k + 1.0f, rect.bottom}, WithAlpha(rgb, t));
        }
        const float kx = rect.left + std::clamp(alpha, 0.0f, 1.0f) * rect.Width();
        context.AddRect(sky::ui::UIRect{kx - 2.0f, rect.top - 1.0f, kx + 2.0f, rect.bottom + 1.0f}, RGB(0xFF, 0xFF, 0xFF));
    }

    void ScrollBar(sky::ui::UIPaintContext &context, const sky::ui::UIRect &track, float contentHeight, float scrollTop, uint32_t thumbColor)
    {
        if (contentHeight <= track.Height() || track.Height() <= 0.0f) {
            return;
        }
        const float           ratio     = std::clamp(track.Height() / contentHeight, 0.08f, 1.0f);
        const float           thumbH    = track.Height() * ratio;
        const float           maxScroll = std::max(0.0f, contentHeight - track.Height());
        const float           t         = maxScroll > 0.0f ? std::clamp(scrollTop / maxScroll, 0.0f, 1.0f) : 0.0f;
        const float           y         = track.top + (track.Height() - thumbH) * t;
        const sky::ui::UIRect thumb{track.right - 8.0f, y, track.right - 4.0f, y + thumbH};
        RoundedRect(context, thumb, thumbColor, 2.0f);
    }

} // namespace sky::editor::uidraw
