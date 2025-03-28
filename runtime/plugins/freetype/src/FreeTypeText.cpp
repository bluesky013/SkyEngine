//
// Created by blues on 2024/9/11.
//

#include <freetype/FreeTypeText.h>
#include <render/text/TextFeature.h>
#include <render/RenderScene.h>
#include <core/math/Vector2.h>

namespace sky {

    struct TextTransform {
        Vector2 scale;
        Vector2 translate;
    };

    static std::vector<VertexAttribute> TEXT_ATTRIBUTES = {
            VertexAttribute{VertexSemanticFlagBit::POSITION, 0, OFFSET_OF(TextVertex, pos), rhi::Format::F_RG32},
            VertexAttribute{VertexSemanticFlagBit::UV,       0, OFFSET_OF(TextVertex, uv),  rhi::Format::F_RG32},
            VertexAttribute{VertexSemanticFlagBit::COLOR,    0, OFFSET_OF(TextVertex, col), rhi::Format::F_RGBA32},
    };

    static VertexSemanticFlags TEXT_VTX_SEMANTICS = VertexSemanticFlagBit::POSITION |
            VertexSemanticFlagBit::UV |
            VertexSemanticFlagBit::COLOR;

    void TextPrimitive::UpdateBatch()
    {
        auto &batch = batches[0];
        if (!batch.batchGroup && batch.program) {
            auto layout = batch.program->RequestLayout(BATCH_SET);
            batch.batchGroup = TextFeature::Get()->RequestResourceGroup();
            batch.batchGroup->BindTexture(Name("FontTexture"), fontTexture->GetImageView(), 0);
            batch.batchGroup->BindDynamicUBO(Name("Constants"), ubo, 0);
            batch.batchGroup->Update();
        }
    }

    void TextBatch::Init(const RDGfxTechPtr &tech, const RDTexturePtr &tex, const RDDynamicUniformBufferPtr &ubo)
    {
        primitive = std::make_unique<TextPrimitive>();
        primitive->ubo = ubo;
        primitive->fontTexture = tex;
        primitive->batches.emplace_back(RenderBatch(tech));
    }

    void TextBatch::Flush()
    {
        primitive->geometry = new RenderGeometry();
        primitive->geometry->vertexAttributes   = TEXT_ATTRIBUTES;
        primitive->geometry->attributeSemantics = TEXT_VTX_SEMANTICS;
        primitive->geometry->version++;
        primitive->args.clear();
        primitive->args.emplace_back(args);

        auto *buffer = new Buffer();
        buffer->Init(vertices.size() * sizeof(TextVertex), rhi::BufferUsageFlagBit::VERTEX, rhi::MemoryType::CPU_TO_GPU);
        auto *ptr = buffer->GetRHIBuffer()->Map();
        memcpy(ptr, vertices.data(),  buffer->GetSize());

        primitive->geometry->vertexBuffers.emplace_back(VertexBuffer{
            buffer, 0, buffer->GetSize(), sizeof(TextVertex)
        });
    }

    void TextBatch::AddQuad(const Rect &rect, const Rect &uv, const Color &color)
    {
        TextVertex quad[4] = {
            {{rect.offset.x             , rect.offset.y             }, {uv.offset.x           , uv.offset.y           }, color},
            {{rect.offset.x + rect.ext.x, rect.offset.y             }, {uv.offset.x + uv.ext.x, uv.offset.y           }, color},
            {{rect.offset.x             , rect.offset.y + rect.ext.y}, {uv.offset.x           , uv.offset.y + uv.ext.y}, color},
            {{rect.offset.x + rect.ext.x, rect.offset.y + rect.ext.y}, {uv.offset.x + uv.ext.x, uv.offset.y + uv.ext.y}, color},
        };

        vertices.emplace_back(quad[0]);
        vertices.emplace_back(quad[1]);
        vertices.emplace_back(quad[2]);

        vertices.emplace_back(quad[3]);
        vertices.emplace_back(quad[2]);
        vertices.emplace_back(quad[1]);

        args.vertexCount += 6;
    }

    void FreeTypeText::Reset(RenderScene& scene)
    {
        for (auto &batch : batches) {
            scene.RemovePrimitive(batch.second->primitive.get());
        }
        batches.clear();
    }

    void FreeTypeText::Finalize(RenderScene& scene)
    {
        for (auto &[key, batch] : batches) {
            batch->Flush();
            scene.AddPrimitive(batch->primitive.get());
        }
    }

    bool FreeTypeText::Init(const TextDesc &desc)
    {
        ubo = new DynamicUniformBuffer();
        ubo->Init(static_cast<uint32_t>(sizeof(TextTransform)));

        return static_cast<FreeTypeFont*>(font.Get())->Init(desc.fontSize, desc.texWidth, desc.texHeight);
    }

    TextFlags FreeTypeText::ValidateFlags(const TextFlags &flags)
    {
        return flags & TextFlags(0x11);
    }

    void FreeTypeText::SetDisplaySize(float w, float h)
    {
        TextTransform transform = {};
        transform.scale.x = 2.0f / w;
        transform.scale.y = 2.0f / h;
        transform.translate.x = -1.0f;
        transform.translate.y = -1.0f;
        ubo->WriteT(0, transform);
        ubo->Upload();
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
                    offsetY + static_cast<float>(-glyph->bearingY) * info.scale,
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
            if (!batch) {
                batch = new TextBatch();
                batch->Init(TextFeature::Get()->GetTechnique(), ftFont->GetTextureByIndex(glyph->index), ubo);
            }

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