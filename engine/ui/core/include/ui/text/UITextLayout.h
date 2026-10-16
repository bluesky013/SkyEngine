//
// Created on 2026/09/19.
//

#pragma once

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace sky::ui {

    class UIFontAtlas;
    class UIPaintContext;

    struct UITextExtent {
        float width  = 0.0f;
        float height = 0.0f;
    };

    enum class UITextLineAlign : uint8_t {
        LEFT = 0,
        CENTER,
        RIGHT,
    };

    struct UITextLine {
        std::string text;
        float       width = 0.0f;
    };

    // Runtime layout result shared by measurement and emission. Text remains
    // byte-based, matching the current font/layout contract; explicit blank
    // lines are retained and wrapping never splits a glyph into smaller pieces.
    struct UITextLayoutData {
        std::vector<UITextLine> lines;
        UITextExtent            extent;
        float                   lineHeight = 0.0f;
        uint32_t                fontSize   = 0;
    };

    class UITextLayout {
    public:
        // With wrapping enabled, finite maxWidth soft-wraps before a byte whose
        // advance would exceed the line. A single oversized glyph gets its own
        // line. Infinite width preserves the explicit-newline-only behavior.
        static UITextLayoutData
        Build(const std::string &text, uint32_t size, UIFontAtlas &atlas, float maxWidth = std::numeric_limits<float>::infinity(), bool wrap = false);

        // Explicit newlines only: width = longest line advance sum, height = lines x line height.
        static UITextExtent Measure(const std::string &text, uint32_t size, UIFontAtlas &atlas);

        // Emits an already measured layout. Alignment is applied per line;
        // without a finite lineWidth it uses the longest line's advance width.
        static void Emit(UIPaintContext         &context,
                         const UITextLayoutData &layout,
                         float                   x,
                         float                   y,
                         uint32_t                color,
                         UIFontAtlas            &atlas,
                         float                   lineWidth = std::numeric_limits<float>::infinity(),
                         UITextLineAlign         align     = UITextLineAlign::LEFT);

        // Emits glyph quads referencing the atlas handle through the paint context,
        // so the current clip stack applies.
        static void Emit(UIPaintContext &context, const std::string &text, uint32_t size, float x, float y, uint32_t color, UIFontAtlas &atlas);
    };

} // namespace sky::ui
