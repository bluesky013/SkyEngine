//
// Created on 2026/09/19.
//

#include <ui/text/UIFontAtlas.h>

#include <algorithm>

namespace sky::ui {

    namespace {

        const UIGlyphEntry &EmptyEntry()
        {
            static const UIGlyphEntry entry;
            return entry;
        }

        uint64_t MakeKey(uint32_t size, uint32_t codepoint)
        {
            return (static_cast<uint64_t>(size) << 32) | codepoint;
        }

    } // namespace

    UIFontAtlas::UIFontAtlas(IUITextureRegistry *registry, uint32_t pageSize)
        : registry(registry)
        , pageSize(std::max<uint32_t>(pageSize, 1))
    {
    }

    UIFontAtlas::Page *UIFontAtlas::AcquirePage(uint32_t width, uint32_t height)
    {
        // A glyph larger than a page cannot be packed; report failure instead of
        // writing past the page buffer.
        if (width > pageSize || height > pageSize) {
            return nullptr;
        }

        for (auto &page : pages) {
            if (page.cursorX + width <= pageSize && page.cursorY + height <= pageSize) {
                return &page;
            }
            if (page.cursorY + page.rowHeight + height <= pageSize) {
                page.cursorX = 0;
                page.cursorY += page.rowHeight;
                page.rowHeight = 0;
                return &page;
            }
        }

        Page page;
        page.image.width = pageSize;
        page.image.height = pageSize;
        page.image.pixels.assign(static_cast<size_t>(pageSize) * pageSize * 4, 0);
        page.textureId = registry != nullptr ? registry->RegisterTexture(page.image) : UI_INVALID_TEXTURE;
        pages.push_back(std::move(page));
        return &pages.back();
    }

    const UIGlyphEntry &UIFontAtlas::GetGlyph(uint32_t codepoint, uint32_t size)
    {
        if (provider == nullptr) {
            return EmptyEntry();
        }

        const uint64_t key = MakeKey(size, codepoint);
        const auto cached = cache.find(key);
        if (cached != cache.end()) {
            return cached->second;
        }

        UIGlyphBitmap glyph;
        if (!provider->GetGlyph(codepoint, size, glyph)) {
            return EmptyEntry();
        }

        UIGlyphEntry entry;
        entry.advance = glyph.advance;
        entry.bearingX = glyph.bearingX;
        entry.bearingY = glyph.bearingY;
        entry.width = static_cast<float>(glyph.width);
        entry.height = static_cast<float>(glyph.height);

        if (glyph.width > 0 && glyph.height > 0) {
            Page *page = AcquirePage(glyph.width, glyph.height);
            if (page != nullptr) {
                const uint32_t x = page->cursorX;
                const uint32_t y = page->cursorY;

                for (uint32_t row = 0; row < glyph.height; ++row) {
                    for (uint32_t col = 0; col < glyph.width; ++col) {
                        const uint32_t coverage = glyph.pixels[static_cast<size_t>(row) * glyph.width + col];
                        const size_t offset = (static_cast<size_t>(y + row) * pageSize + (x + col)) * 4;
                        page->image.pixels[offset + 0] = 255;
                        page->image.pixels[offset + 1] = 255;
                        page->image.pixels[offset + 2] = 255;
                        page->image.pixels[offset + 3] = static_cast<uint8_t>(coverage);
                    }
                }

                page->rowHeight = std::max(page->rowHeight, glyph.height);
                page->cursorX += glyph.width;

                if (registry != nullptr) {
                    registry->UpdateTexture(page->textureId, page->image);
                }

                const float inv = 1.0f / static_cast<float>(pageSize);
                entry.textureId = page->textureId;
                entry.uv.left = static_cast<float>(x) * inv;
                entry.uv.top = static_cast<float>(y) * inv;
                entry.uv.right = static_cast<float>(x + glyph.width) * inv;
                entry.uv.bottom = static_cast<float>(y + glyph.height) * inv;
            }
        }

        const auto inserted = cache.emplace(key, entry);
        return inserted.first->second;
    }

} // namespace sky::ui
