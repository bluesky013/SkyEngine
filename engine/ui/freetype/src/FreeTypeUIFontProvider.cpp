//
// Created on 2026/09/19.
//

#include <ui/freetype/FreeTypeUIFontProvider.h>

#include <core/file/FileIO.h>
#include <core/logger/Logger.h>

#include <freetype/freetype.h>

#include <cstdlib>

static const char *TAG = "UIFreeType";

namespace sky::ui {

    FreeTypeUIFontProvider::FreeTypeUIFontProvider() = default;

    FreeTypeUIFontProvider::~FreeTypeUIFontProvider()
    {
        if (face != nullptr) {
            FT_Done_Face(face);
            face = nullptr;
        }
        if (library != nullptr) {
            FT_Done_FreeType(library);
            library = nullptr;
        }
    }

    bool FreeTypeUIFontProvider::LoadFont(const std::string &fontPath)
    {
        std::vector<uint8_t> bytes;
        if (!ReadBin(FilePath(fontPath), bytes) || bytes.empty()) {
            LOG_W(TAG, "failed to read font file: %s", fontPath.c_str());
            return false;
        }

        if (library == nullptr && FT_Init_FreeType(&library) != 0) {
            LOG_E(TAG, "failed to init FreeType");
            return false;
        }

        fontBytes = std::move(bytes);
        if (FT_New_Memory_Face(library, fontBytes.data(), static_cast<FT_Long>(fontBytes.size()), 0, &face) != 0) {
            LOG_E(TAG, "failed to create memory face: %s", fontPath.c_str());
            face = nullptr;
            return false;
        }

        activeSize = 0;
        cache.clear();
        return true;
    }

    bool FreeTypeUIFontProvider::EnsureSize(uint32_t size)
    {
        if (face == nullptr || size == 0) {
            return false;
        }
        if (activeSize == size) {
            return true;
        }
        if (FT_Set_Pixel_Sizes(face, 0, size) != 0) {
            return false;
        }
        activeSize = size;
        return true;
    }

    bool FreeTypeUIFontProvider::GetGlyph(uint32_t codepoint, uint32_t size, UIGlyphBitmap &out)
    {
        out = UIGlyphBitmap{};
        if (face == nullptr) {
            return false;
        }

        const uint64_t key = (static_cast<uint64_t>(size) << 32) | codepoint;
        const auto cached = cache.find(key);
        if (cached != cache.end()) {
            out = cached->second.bitmap;
            return cached->second.valid;
        }

        CacheEntry entry;
        if (!EnsureSize(size)) {
            cache.emplace(key, entry);
            return false;
        }

        const FT_UInt index = FT_Get_Char_Index(face, codepoint);
        if (index == 0 || FT_Load_Glyph(face, index, FT_LOAD_RENDER) != 0) {
            cache.emplace(key, entry);
            return false;
        }

        const FT_GlyphSlot slot = face->glyph;
        entry.bitmap.width = slot->bitmap.width;
        entry.bitmap.height = slot->bitmap.rows;
        entry.bitmap.bearingX = static_cast<float>(slot->bitmap_left);
        entry.bitmap.bearingY = static_cast<float>(slot->bitmap_top);
        entry.bitmap.advance = static_cast<float>(slot->advance.x) / 64.0f;

        if (slot->bitmap.width > 0 && slot->bitmap.rows > 0) {
            entry.bitmap.pixels.resize(static_cast<size_t>(slot->bitmap.width) * slot->bitmap.rows);
            const int pitch = std::abs(slot->bitmap.pitch);
            for (uint32_t row = 0; row < slot->bitmap.rows; ++row) {
                for (uint32_t col = 0; col < slot->bitmap.width; ++col) {
                    entry.bitmap.pixels[static_cast<size_t>(row) * slot->bitmap.width + col] =
                        slot->bitmap.buffer[row * pitch + col];
                }
            }
        }

        entry.valid = true;
        out = entry.bitmap;
        cache.emplace(key, std::move(entry));
        return true;
    }

    UIFontMetrics FreeTypeUIFontProvider::GetFontMetrics(uint32_t size)
    {
        UIFontMetrics metrics;
        if (face == nullptr || !EnsureSize(size)) {
            return metrics;
        }
        metrics.ascender = static_cast<float>(face->size->metrics.ascender) / 64.0f;
        metrics.descender = -static_cast<float>(face->size->metrics.descender) / 64.0f;
        metrics.lineHeight = static_cast<float>(face->size->metrics.height) / 64.0f;
        return metrics;
    }

} // namespace sky::ui
