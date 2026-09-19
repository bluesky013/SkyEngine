//
// Created on 2026/09/19.
//

#include <ui/text/UITextLayout.h>
#include <ui/text/UIFontAtlas.h>
#include <ui/UIPaintContext.h>

#include <algorithm>

namespace sky::ui {

    UITextExtent UITextLayout::Measure(const std::string &text, uint32_t size, UIFontAtlas &atlas)
    {
        UITextExtent extent;
        IUIFontProvider *provider = atlas.GetProvider();
        if (provider == nullptr) {
            return extent;
        }

        const float lineHeight = provider->GetFontMetrics(size).lineHeight;
        float lineWidth = 0.0f;
        float maxWidth = 0.0f;
        uint32_t lines = 1;

        for (char ch : text) {
            if (ch == '\n') {
                maxWidth = std::max(maxWidth, lineWidth);
                lineWidth = 0.0f;
                lines++;
                continue;
            }
            const UIGlyphEntry &glyph = atlas.GetGlyph(static_cast<uint32_t>(static_cast<unsigned char>(ch)), size);
            lineWidth += glyph.advance;
        }
        maxWidth = std::max(maxWidth, lineWidth);

        extent.width = maxWidth;
        extent.height = lineHeight * static_cast<float>(lines);
        return extent;
    }

    void UITextLayout::Emit(UIPaintContext &context,
                            const std::string &text,
                            uint32_t size,
                            float x,
                            float y,
                            uint32_t color,
                            UIFontAtlas &atlas)
    {
        IUIFontProvider *provider = atlas.GetProvider();
        if (provider == nullptr) {
            return;
        }

        const float lineHeight = provider->GetFontMetrics(size).lineHeight;
        float penX = x;
        float penY = y;

        for (char ch : text) {
            if (ch == '\n') {
                penX = x;
                penY += lineHeight;
                continue;
            }

            const UIGlyphEntry &glyph = atlas.GetGlyph(static_cast<uint32_t>(static_cast<unsigned char>(ch)), size);
            if (glyph.width > 0.0f && glyph.height > 0.0f && glyph.textureId != UI_INVALID_TEXTURE) {
                UIRect quad;
                quad.left = penX + glyph.bearingX;
                quad.top = penY + (lineHeight - glyph.bearingY);
                quad.right = quad.left + glyph.width;
                quad.bottom = quad.top + glyph.height;
                context.AddTexturedQuad(quad, glyph.uv, glyph.textureId, color);
            }
            penX += glyph.advance;
        }
    }

} // namespace sky::ui
