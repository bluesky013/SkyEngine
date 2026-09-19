//
// Created on 2026/09/19.
//

#pragma once

#include <cstdint>
#include <string>

namespace sky::ui {

    class UIFontAtlas;
    class UIPaintContext;

    struct UITextExtent {
        float width = 0.0f;
        float height = 0.0f;
    };

    class UITextLayout {
    public:
        // Explicit newlines only: width = longest line advance sum, height = lines x line height.
        static UITextExtent Measure(const std::string &text, uint32_t size, UIFontAtlas &atlas);

        // Emits glyph quads referencing the atlas handle through the paint context,
        // so the current clip stack applies.
        static void Emit(UIPaintContext &context,
                         const std::string &text,
                         uint32_t size,
                         float x,
                         float y,
                         uint32_t color,
                         UIFontAtlas &atlas);
    };

} // namespace sky::ui
