//
// Created by blues on 2024/9/11.
//

#pragma once

#include <core/math/Vector2.h>
#include <core/shapes/Base.h>
#include <render/text/Text.h>
#include <freetype/freetype.h>
#include <freetype/FreeTypeFont.h>
#include <memory>
#include <vector>
#include <unordered_map>
#include <sstream>

namespace sky {

    struct TextVertex {
        Vector2 pos;
        Vector2 uv;
        Color   col;
    };

    struct TextBatch : public RefObject {
        void Init(const RDGfxTechPtr &tech, const RDTexturePtr &tex, const RDDynamicUniformBufferPtr &ubo);
        void Flush();
        void AddQuad(const Rect& rect, const Rect &uv, const Color &color);

        rhi::CmdDrawLinear args;
        std::unique_ptr<RenderPrimitive> primitive;
        std::vector<TextVertex> vertices;
    };
    using TextBatchPtr = CounterPtr<TextBatch>;

    class FreeTypeText : public Text {
    public:
        explicit FreeTypeText(const FontPtr &font_) : Text(font_) {}
        ~FreeTypeText() override = default;

    private:
        bool Init(const TextDesc &desc) override;
        void SetDisplaySize(float w, float h) override;

        void Reset(RenderScene& scene) override;
        void Finalize(RenderScene& scene) override;

        void AddText(const std::string &text, const Vector2& pos, const TextInfo &info) override;

        static TextFlags ValidateFlags(const TextFlags &flags) ;
        std::unordered_map<BatchKey, TextBatchPtr> batches;
        RDDynamicUniformBufferPtr ubo;
    };

} // namespace sky
