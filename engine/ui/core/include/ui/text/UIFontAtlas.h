//
// Created on 2026/09/19.
//

#pragma once

#include <ui/IUIFontProvider.h>
#include <ui/IUITextureRegistry.h>
#include <ui/UIRect.h>

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace sky::ui {

    struct UIGlyphEntry {
        UITextureId textureId = UI_INVALID_TEXTURE;
        UIRect uv;
        float width = 0.0f;
        float height = 0.0f;
        float bearingX = 0.0f;
        float bearingY = 0.0f;
        float advance = 0.0f;
    };

    // Caches rasterized glyphs by (size, codepoint), packs them into fixed-size
    // pages, and registers each page once through IUITextureRegistry. A full page
    // grows the atlas with a new page; existing pages and handles stay valid.
    class UIFontAtlas {
    public:
        UIFontAtlas(IUITextureRegistry *registry, uint32_t pageSize = 256);

        void SetProvider(IUIFontProvider *value) { provider = value; }
        IUIFontProvider *GetProvider() const { return provider; }

        const UIGlyphEntry &GetGlyph(uint32_t codepoint, uint32_t size);
        uint32_t GetPageCount() const { return static_cast<uint32_t>(pages.size()); }

    private:
        struct Page {
            UIImageData image;
            UITextureId textureId = UI_INVALID_TEXTURE;
            uint32_t cursorX = 0;
            uint32_t cursorY = 0;
            uint32_t rowHeight = 0;
        };

        Page *AcquirePage(uint32_t width, uint32_t height);

        IUITextureRegistry *registry = nullptr;
        IUIFontProvider *provider = nullptr;
        uint32_t pageSize = 256;
        std::vector<Page> pages;
        std::unordered_map<uint64_t, UIGlyphEntry> cache;
    };

} // namespace sky::ui
