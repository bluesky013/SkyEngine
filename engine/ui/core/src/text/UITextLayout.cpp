//
// Created on 2026/09/19.
//

#include <ui/UIPaintContext.h>
#include <ui/text/UIFontAtlas.h>
#include <ui/text/UITextLayout.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace sky::ui {

    UITextLayoutData UITextLayout::Build(const std::string &text, uint32_t size, UIFontAtlas &atlas, float maxWidth, bool wrap)
    {
        UITextLayoutData layout;
        layout.fontSize           = size;
        IUIFontProvider *provider = atlas.GetProvider();
        if (provider == nullptr) {
            return layout;
        }

        layout.lineHeight = provider->GetFontMetrics(size).lineHeight;
        layout.lines.emplace_back();
        const bool  bounded = wrap && std::isfinite(maxWidth);
        const float limit   = std::max(0.0f, maxWidth);
        // Only account for relative glyph/padding arithmetic. Callers preserve
        // logical allocation width instead of subtracting positioned edges;
        // absolute origin must never enlarge this tolerance or change wrapping.
        const double wrapTolerance = static_cast<double>(limit) * std::numeric_limits<float>::epsilon() * 8.0;
        for (char ch : text) {
            if (ch == '\n') {
                layout.lines.emplace_back();
                continue;
            }
            const UIGlyphEntry &glyph     = atlas.GetGlyph(static_cast<uint32_t>(static_cast<unsigned char>(ch)), size);
            const double        nextWidth = static_cast<double>(layout.lines.back().width) + glyph.advance;
            if (bounded && !layout.lines.back().text.empty() && nextWidth > static_cast<double>(limit) + wrapTolerance) {
                layout.lines.emplace_back();
            }
            UITextLine &line = layout.lines.back();
            line.text.push_back(ch);
            line.width += glyph.advance;
        }
        for (const auto &line : layout.lines) {
            layout.extent.width = std::max(layout.extent.width, line.width);
        }
        layout.extent.height = layout.lineHeight * static_cast<float>(layout.lines.size());
        return layout;
    }

    UITextExtent UITextLayout::Measure(const std::string &text, uint32_t size, UIFontAtlas &atlas)
    {
        return Build(text, size, atlas).extent;
    }

    void UITextLayout::Emit(UIPaintContext         &context,
                            const UITextLayoutData &layout,
                            float                   x,
                            float                   y,
                            uint32_t                color,
                            UIFontAtlas            &atlas,
                            float                   lineWidth,
                            UITextLineAlign         align)
    {
        const float availableWidth = std::isfinite(lineWidth) ? std::max(0.0f, lineWidth) : layout.extent.width;
        float       penY           = y;
        for (const auto &line : layout.lines) {
            float penX = x;
            if (align == UITextLineAlign::CENTER) {
                penX += (availableWidth - line.width) * 0.5f;
            } else if (align == UITextLineAlign::RIGHT) {
                penX += availableWidth - line.width;
            }

            for (char ch : line.text) {
                const UIGlyphEntry &glyph = atlas.GetGlyph(static_cast<uint32_t>(static_cast<unsigned char>(ch)), layout.fontSize);
                if (glyph.width > 0.0f && glyph.height > 0.0f && glyph.textureId != UI_INVALID_TEXTURE) {
                    // Snap each glyph quad to integer pixels so linear sampling stays
                    // texel-aligned (subpixel placement blurs bitmap text).
                    const float  gx = std::round(penX + glyph.bearingX);
                    const float  gy = std::round(penY + (layout.lineHeight - glyph.bearingY));
                    const UIRect quad{gx, gy, gx + glyph.width, gy + glyph.height};
                    context.AddTexturedQuad(quad, glyph.uv, glyph.textureId, color);
                }
                penX += glyph.advance;
            }
            penY += layout.lineHeight;
        }
    }

    void UITextLayout::Emit(UIPaintContext &context, const std::string &text, uint32_t size, float x, float y, uint32_t color, UIFontAtlas &atlas)
    {
        Emit(context, Build(text, size, atlas), x, y, color, atlas);
    }

} // namespace sky::ui
