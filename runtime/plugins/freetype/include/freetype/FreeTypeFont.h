//
// Created by blues on 2024/9/11.
//

#pragma once

#include <render/text/Font.h>
#include <render/resource/TextureAtlas.h>
#include <freetype/freetype.h>
#include <memory>
#include <atomic>
#include <vector>
#include <unordered_map>

namespace sky {
    struct FreeTypeGlyph {
        uint32_t width    = 0;
        uint32_t height   = 0;
        int32_t  bearingX = 0;
        int32_t  bearingY = 0;
        int32_t  advanceX = 0;

        uint32_t x        = 0;
        uint32_t y        = 0;
        uint32_t index    = 0;
    };

    class FreeTypeFont : public Font {
    public:
        explicit FreeTypeFont(const std::string &name) : Font(name) {}
        ~FreeTypeFont() override;

        void Load(const FilePtr &file) override;
        bool Init(uint32_t fontSize, uint32_t fontWidth, uint32_t fontHeight);

        FreeTypeGlyph* Query(uint32_t code);
        FreeTypeGlyph* Allocate(uint32_t code);

        uint32_t GetLineHeight() const { return lineHeight; }
        uint32_t GetFontTexWidth() const { return fontWidth; }
        uint32_t GetFontTexHeight() const { return fontHeight; }

        RDTexture2DPtr GetTextureByIndex(uint32_t index);
    private:
        bool IsReady() const override;

        void EmplaceTexture();

        FT_Face face;
        uint32_t lineHeight = 0;
        uint32_t fontWidth = 512;
        uint32_t fontHeight = 512;

        std::vector<RDTexture2DAtlasPtr> textures;
        std::unordered_map<uint32_t, FreeTypeGlyph> glyphLut;
        std::vector<uint8_t> rawData;
    };

} // namespace sky
