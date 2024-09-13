//
// Created by blues on 2024/9/11.
//

#include <freetype/FreeTypeText.h>

namespace sky {

    void TextBatch::AddQuad(const Rect &rect, const Rect &uv, const Color &color)
    {
        TextVertex quad[4] = {
            {{rect.offset.x             , rect.offset.y             }, {uv.offset.x           , uv.offset.y           }, color},
            {{rect.offset.x + rect.ext.x, rect.offset.y             }, {uv.offset.x + uv.ext.y, uv.offset.y           }, color},
            {{rect.offset.x             , rect.offset.y + rect.ext.y}, {uv.offset.x           , uv.offset.y + uv.ext.y}, color},
            {{rect.offset.x + rect.ext.x, rect.offset.y + rect.ext.y}, {uv.offset.x + uv.ext.y, uv.offset.y + uv.ext.y}, color},
        };

        vertices.emplace_back(quad[0]);
        vertices.emplace_back(quad[1]);
        vertices.emplace_back(quad[2]);

        vertices.emplace_back(quad[3]);
        vertices.emplace_back(quad[2]);
        vertices.emplace_back(quad[1]);
    }

    void FreeTypeText::Reset()
    {
        batches.clear();
    }

    bool FreeTypeText::Init(const TextDesc &desc)
    {
        return static_cast<FreeTypeFont*>(font.Get())->Init(desc.fontSize, desc.texWidth, desc.texHeight);
    }

    TextFlags FreeTypeText::ValidateFlags(const TextFlags &flags)
    {
        return flags & TextFlags(0x11);
    }

    void FreeTypeText::AddText(const std::string &text, const Vector2& pos, const TextInfo &info)
    {
        if (text.empty()) {
            return;
        }

        auto *ftFont = static_cast<FreeTypeFont*>(font.Get());

        auto offsetX = pos.x;
        auto offsetY = pos.y;
        auto lingHeight = static_cast<float>(ftFont->GetLineHeight()) * info.scale;

        for (const auto &ch : text) {
            if (ch == '\n') {
                offsetX = pos.x;
                offsetY += lingHeight;
                continue;
            }

            auto *glyph = ftFont->Query(ch);
            if (glyph == nullptr) {
                continue;
            }

            Rect position = {
                Vector2{
                    offsetX + static_cast<float>(glyph->bearingX) * info.scale,
                    offsetY + static_cast<float>(glyph->bearingY) * info.scale,
                },
                Vector2{
                    static_cast<float>(glyph->width) * info.scale,
                    static_cast<float>(glyph->height) * info.scale,
                }
            };

            float invTexWidth =  1.f / static_cast<float>(ftFont->GetFontTexWidth());
            float invTexHeight = 1.f / static_cast<float>(ftFont->GetFontTexHeight());
            Rect uv = {
                Vector2{
                    static_cast<float>(glyph->x) * invTexWidth,
                    static_cast<float>(glyph->y) * invTexHeight,
                },
                Vector2{
                    static_cast<float>(glyph->width) * invTexWidth,
                    static_cast<float>(glyph->height) * invTexHeight,
                }
            };

            auto &batch = batches[BatchKey{ValidateFlags(info.flags), glyph->index}];
            if (info.flags & TextFlagBit::SHADOW) {
                for (uint32_t i = 1; i <= info.shadowThickness; ++i) {
                    for (uint32_t j = 1; j <= info.shadowThickness; ++j) {
                        Rect shadowPos = position;
                        shadowPos.offset.x += static_cast<float>(i);
                        shadowPos.offset.y += static_cast<float>(j);

                        batch->AddQuad(shadowPos, uv, info.shadowColor);
                    }
                }

            }
            batch->AddQuad(position, uv, info.color);

            offsetX += static_cast<float>(glyph->advanceX) * info.scale;
        }
    }

} // namespace sky