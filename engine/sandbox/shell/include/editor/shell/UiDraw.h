//
// Created on 2026/10/04.
//

#pragma once

#include <ui/UIRect.h>
#include <cstdint>
#include <string>

namespace sky::ui {
    class UIPaintContext;
    class UITextSystem;
} // namespace sky::ui

namespace sky::editor::uidraw {

    constexpr uint32_t RGB(uint32_t r, uint32_t g, uint32_t b, uint32_t a = 0xFF)
    {
        return (a << 24) | (b << 16) | (g << 8) | r;
    }

    // Cohesive dark palette (VS Code / Unity / Godot inspired).
    namespace color {
        constexpr uint32_t Window      = RGB(0x1E, 0x1E, 0x1E);
        constexpr uint32_t Panel       = RGB(0x25, 0x25, 0x26);
        constexpr uint32_t Header      = RGB(0x2D, 0x2D, 0x30);
        constexpr uint32_t HeaderTop   = RGB(0x39, 0x39, 0x3E);
        constexpr uint32_t Section     = RGB(0x2A, 0x2A, 0x2D);
        constexpr uint32_t SectionTop  = RGB(0x31, 0x31, 0x35);
        constexpr uint32_t AccentSoft  = RGB(0x14, 0x7A, 0xC2);
        constexpr uint32_t RowEven     = RGB(0x25, 0x25, 0x26);
        constexpr uint32_t RowOdd      = RGB(0x2A, 0x2A, 0x2C);
        constexpr uint32_t RowHover    = RGB(0x37, 0x37, 0x3D);
        constexpr uint32_t RowSelected = RGB(0x09, 0x47, 0x71);
        constexpr uint32_t Border      = RGB(0x3F, 0x3F, 0x46);
        constexpr uint32_t BorderSoft  = RGB(0x33, 0x33, 0x36);
        constexpr uint32_t Text        = RGB(0xDC, 0xDC, 0xDC);
        constexpr uint32_t TextMuted   = RGB(0x9A, 0x9A, 0x9A);
        constexpr uint32_t TextDisabled= RGB(0x6A, 0x6A, 0x6A);
        constexpr uint32_t Accent      = RGB(0x0E, 0x63, 0x9C);
        constexpr uint32_t AccentHover = RGB(0x11, 0x77, 0xBB);
        constexpr uint32_t Field       = RGB(0x1B, 0x1B, 0x1C);
        constexpr uint32_t FieldHover  = RGB(0x26, 0x26, 0x29);
        constexpr uint32_t SliderTrack = RGB(0x14, 0x14, 0x15);
        constexpr uint32_t SliderFill  = RGB(0x0E, 0x63, 0x9C);
        constexpr uint32_t SliderHandle= RGB(0xD0, 0xD0, 0xD0);
        constexpr uint32_t CheckOff    = RGB(0x3A, 0x3A, 0x3C);
        constexpr uint32_t CheckOn     = RGB(0x0E, 0x63, 0x9C);
        constexpr uint32_t Toolbar     = RGB(0x32, 0x32, 0x35);
        constexpr uint32_t TabActive   = RGB(0x1E, 0x1E, 0x1E);
        constexpr uint32_t TabInactive = RGB(0x2D, 0x2D, 0x30);
        constexpr uint32_t Guide       = RGB(0x30, 0x30, 0x33);
        constexpr uint32_t ScrollBar   = RGB(0x5A, 0x5A, 0x5F);
        constexpr uint32_t White       = RGB(0xFF, 0xFF, 0xFF);
    } // namespace color

    enum class HAlign { Left, Center, Right };
    enum class VAlign { Top, Middle, Bottom };

    float TextWidth(const std::string &text, uint32_t size, sky::ui::UITextSystem *textSystem);
    float TextHeight(const std::string &text, uint32_t size, sky::ui::UITextSystem *textSystem);

    // Truncates with a trailing ellipsis so the text fits within maxWidth.
    std::string Ellipsize(const std::string &text, uint32_t size, float maxWidth, sky::ui::UITextSystem *textSystem);

    uint32_t LerpColor(uint32_t top, uint32_t bottom, float t);

    void Fill(sky::ui::UIPaintContext &context, const sky::ui::UIRect &rect, uint32_t color);
    void Border(sky::ui::UIPaintContext &context, const sky::ui::UIRect &rect, uint32_t color, float thickness = 1.0f);
    void HLine(sky::ui::UIPaintContext &context, float x0, float x1, float y, uint32_t color);

    // Anti-aliased-ish rounded rectangle (scanline approximation).
    void RoundedRect(sky::ui::UIPaintContext &context, const sky::ui::UIRect &rect, uint32_t color, float radius);
    void RoundedGradient(sky::ui::UIPaintContext &context, const sky::ui::UIRect &rect, uint32_t topColor,
                         uint32_t bottomColor, float radius);
    // Rounded border with a solid inner fill (crisp fields/buttons).
    void RoundedField(sky::ui::UIPaintContext &context, const sky::ui::UIRect &rect, uint32_t bg, uint32_t border,
                      float radius = 4.0f);
    // Faux drop shadow: a few translucent rounded rects offset below.
    void SoftShadow(sky::ui::UIPaintContext &context, const sky::ui::UIRect &rect, float radius);

    // 1px inset border drawn inside the rect (keeps fields crisp).
    void Field(sky::ui::UIPaintContext &context, const sky::ui::UIRect &rect, uint32_t bg, uint32_t border);

    // Draws a horizontal slider track with fill/handle for the given value.
    void Slider(sky::ui::UIPaintContext &context, const sky::ui::UIRect &rect, float value, float minV, float maxV,
                uint32_t fillColor);

    // --- Color helpers (ABGR packed in/out, h/s/v/a in [0,1]) ---
    uint32_t HsvToRgb(float h, float s, float v, float a = 1.0f);
    void RgbToHsv(uint32_t argb, float &h, float &s, float &v, float &a);
    uint32_t WithAlpha(uint32_t argb, float a);

    void Checker(sky::ui::UIPaintContext &context, const sky::ui::UIRect &rect, float cell = 8.0f);
    // Blender-style hue ring; markerHue draws the current-hue knob.
    void HueRing(sky::ui::UIPaintContext &context, float cx, float cy, float outerR, float innerR, float value,
                 float markerHue);
    // Saturation (x) / value (y) square for a hue, with a selection knob.
    void ColorSquare(sky::ui::UIPaintContext &context, const sky::ui::UIRect &rect, float hue, float sat, float val);
    // Alpha bar over a checker background, from transparent to opaque rgb.
    void AlphaBar(sky::ui::UIPaintContext &context, const sky::ui::UIRect &rect, uint32_t rgb, float alpha);

    // Draws text within rect, clipped (ellipsis) and aligned; VAlign middle by default.
    void Text(sky::ui::UIPaintContext &context, const std::string &text, uint32_t size,
              const sky::ui::UIRect &rect, uint32_t textColor, sky::ui::UITextSystem *textSystem,
              HAlign hAlign = HAlign::Left, VAlign vAlign = VAlign::Middle, bool ellipsize = true);

    // Draws a thin scrollbar thumb on the right edge of track when content overflows.
    void ScrollBar(sky::ui::UIPaintContext &context, const sky::ui::UIRect &track, float contentHeight,
                   float scrollTop, uint32_t thumbColor);

} // namespace sky::editor::uidraw
