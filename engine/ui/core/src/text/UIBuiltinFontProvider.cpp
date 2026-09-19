//
// Created on 2026/09/19.
//

#include <ui/text/UIBuiltinFontProvider.h>

namespace sky::ui {

    namespace {

        // advance = floor(0.6 * size); integer math keeps measurement exact.
        uint32_t BuiltinAdvance(uint32_t size)
        {
            return (size * 3) / 5;
        }

    } // namespace

    UIFontMetrics UIBuiltinFontProvider::GetFontMetrics(uint32_t size)
    {
        UIFontMetrics metrics;
        metrics.ascender = static_cast<float>(size) * 0.8f;
        metrics.descender = static_cast<float>(size) * 0.2f;
        metrics.lineHeight = static_cast<float>(size);
        return metrics;
    }

    bool UIBuiltinFontProvider::GetGlyph(uint32_t codepoint, uint32_t size, UIGlyphBitmap &out)
    {
        out = UIGlyphBitmap{};
        out.advance = static_cast<float>(BuiltinAdvance(size));

        // Whitespace advances the pen but contributes no coverage.
        if (codepoint == ' ' || codepoint == '\t' || codepoint == '\r') {
            return true;
        }

        if (size == 0) {
            return false;
        }

        const uint32_t width = BuiltinAdvance(size) > 1 ? BuiltinAdvance(size) - 1 : 1;
        const uint32_t height = size;

        out.width = width;
        out.height = height;
        out.bearingX = 0.0f;
        out.bearingY = static_cast<float>(height);
        out.pixels.assign(static_cast<size_t>(width) * height, 255);
        return true;
    }

} // namespace sky::ui
