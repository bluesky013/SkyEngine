//
// Created on 2026/09/19.
//

#pragma once

#include <cstdint>
#include <vector>

namespace sky::ui {

    struct UIGlyphBitmap {
        uint32_t width = 0;
        uint32_t height = 0;
        float bearingX = 0.0f;
        float bearingY = 0.0f;
        float advance = 0.0f;
        std::vector<uint8_t> pixels; // 8-bit coverage
    };

    struct UIFontMetrics {
        float ascender = 0.0f;
        float descender = 0.0f;
        float lineHeight = 0.0f;
    };

    // Core-side seam: text layout asks for glyph bitmaps and metrics. A built-in
    // fallback provider ships with core; a FreeType-backed provider is optional.
    class IUIFontProvider {
    public:
        IUIFontProvider() = default;
        virtual ~IUIFontProvider() = default;

        IUIFontProvider(const IUIFontProvider &) = delete;
        IUIFontProvider &operator=(const IUIFontProvider &) = delete;

        virtual bool GetGlyph(uint32_t codepoint, uint32_t size, UIGlyphBitmap &out) = 0;
        virtual UIFontMetrics GetFontMetrics(uint32_t size) = 0;
    };

} // namespace sky::ui
