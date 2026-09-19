//
// Created on 2026/09/19.
//

#pragma once

#include <ui/IUIFontProvider.h>

namespace sky::ui {

    // Deterministic fallback provider with procedural coverage glyphs, so text
    // works in any build (and in headless tests) without a font file or plugin.
    class UIBuiltinFontProvider : public IUIFontProvider {
    public:
        UIBuiltinFontProvider() = default;
        ~UIBuiltinFontProvider() override = default;

        bool GetGlyph(uint32_t codepoint, uint32_t size, UIGlyphBitmap &out) override;
        UIFontMetrics GetFontMetrics(uint32_t size) override;
    };

} // namespace sky::ui
