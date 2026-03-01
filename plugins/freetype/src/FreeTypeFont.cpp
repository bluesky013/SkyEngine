//
// Created by blues on 2024/9/11.
//

#include <freetype/FreeTypeFont.h>
#include <freetype/FreeTypeLibrary.h>
#include <core/logger/Logger.h>
#include <render/RHI.h>
#include <render/Renderer.h>
static const char* TAG = "FreeType";

namespace sky {

    FreeTypeFont::~FreeTypeFont()
    {
        if (face != nullptr) {
            FT_Done_Face(face);
            face = nullptr;
        }
    }

    void FreeTypeFont::Load(const FilePtr &file)
    {
        auto *library = FreeTypeLibrary::Get()->GetLibrary();
        file->ReadBin(rawData);
        FT_Error error = FT_New_Memory_Face(library, rawData.data(), static_cast<FT_Long>(rawData.size()), 0, &face);
        if (error != 0) {
            LOG_E(TAG, "New Memory Face failed : %d.", error);
        }
    }

    bool FreeTypeFont::Init(uint32_t fontSize, uint32_t w, uint32_t h)
    {
        FT_Error error = FT_Set_Pixel_Sizes(face, 0, fontSize);
        if (error != 0) {
            LOG_E(TAG, "Set Font Size failed : %d.", error);
            return false;
        }

        fontWidth  = w;
        fontHeight = h;
        lineHeight = static_cast<uint32_t>(face->size->metrics.height >> 6);
        return true;
    }

    FreeTypeGlyph* FreeTypeFont::Query(uint32_t code)
    {
        auto iter = glyphLut.find(code);
        if (iter != glyphLut.end()) {
            return &iter->second;
        }

        return Allocate(code);
    }

    RDTexture2DPtr FreeTypeFont::GetTextureByIndex(uint32_t index)
    {
        return textures[index];
    }

    FreeTypeGlyph* FreeTypeFont::Allocate(uint32_t code)
    {
        FT_Error error = FT_Load_Char(face, code, FT_LOAD_RENDER);
        if (error != 0) {
            return nullptr;
        }

        FreeTypeGlyph glyph;
        glyph.width  = face->glyph->bitmap.width;
        glyph.height = face->glyph->bitmap.rows;
        glyph.bearingX = face->glyph->bitmap_left;
        glyph.bearingY = face->glyph->bitmap_top;
        glyph.advanceX = static_cast<int32_t>(face->glyph->advance.x >> 6);

        if (textures.empty()) {
            EmplaceTexture();
        }

        if (glyph.width > 0 && glyph.height > 0) {
            // try allocate
            auto res = textures.back()->Allocate(glyph.width, glyph.height);
            if (!res.first) {
                EmplaceTexture();
                // try again
                res = textures.back()->Allocate(glyph.width, glyph.height);
            }

            if (!res.first) {
                return nullptr;
            }

            glyph.x = res.second.x;
            glyph.y = res.second.y;
            glyph.index = static_cast<uint32_t>(textures.size()) - 1;

            uint32_t dataSize = static_cast<uint32_t>(std::abs(face->glyph->bitmap.pitch)) * face->glyph->bitmap.rows;
            std::vector<uint8_t> data(dataSize);
            memcpy(data.data(), face->glyph->bitmap.buffer, dataSize);
            textures[glyph.index]->Upload(res.second, *RHI::Get()->GetDevice()->GetQueue(rhi::QueueType::TRANSFER), std::move(data));
        }

        auto it = glyphLut.emplace(code, glyph);
        return &it.first->second;
    }

    bool FreeTypeFont::IsReady() const
    {
        return face != nullptr;
    }

    void FreeTypeFont::EmplaceTexture()
    {
        auto *tex = new Texture2DAtlas();
        tex->Init(rhi::PixelFormat::R8_UNORM, fontWidth, fontHeight);

        textures.emplace_back(tex);
    }

} // namespace sky