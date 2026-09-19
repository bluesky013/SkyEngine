//
// Created on 2026/09/19.
//

#pragma once

#include <ui/IUIFontProvider.h>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

struct FT_FaceRec_;
struct FT_LibraryRec_;

namespace sky::ui {

    // FreeType-backed provider for the UI text system, used directly by callers.
    // Part of the UI module; compiled and linked only when SKY_BUILD_FREETYPE is
    // enabled (the built-in fallback provider covers builds without it).
    class FreeTypeUIFontProvider : public IUIFontProvider {
    public:
        FreeTypeUIFontProvider();
        ~FreeTypeUIFontProvider() override;

        FreeTypeUIFontProvider(const FreeTypeUIFontProvider &) = delete;
        FreeTypeUIFontProvider &operator=(const FreeTypeUIFontProvider &) = delete;

        // Reads the font file and creates a memory face. Returns false on failure.
        bool LoadFont(const std::string &fontPath);

        bool IsReady() const { return face != nullptr; }

        bool GetGlyph(uint32_t codepoint, uint32_t size, UIGlyphBitmap &out) override;
        UIFontMetrics GetFontMetrics(uint32_t size) override;

    private:
        struct CacheEntry {
            UIGlyphBitmap bitmap;
            bool valid = false;
        };

        bool EnsureSize(uint32_t size);

        FT_LibraryRec_ *library = nullptr;
        FT_FaceRec_ *face = nullptr;
        // FT_New_Memory_Face references this buffer for the face lifetime.
        std::vector<uint8_t> fontBytes;
        uint32_t activeSize = 0;
        std::unordered_map<uint64_t, CacheEntry> cache;
    };

} // namespace sky::ui
