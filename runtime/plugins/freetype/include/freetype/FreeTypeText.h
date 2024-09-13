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
        Vector2 position;
        Vector2 uv;
        Color   color;
    };

    struct TextBatch : public RefObject {
        void AddQuad(const Rect& rect, const Rect &uv, const Color &color);

        std::vector<TextVertex> vertices;
    };
    using TextBatchPtr = CounterPtr<TextBatch>;

    class FreeTypeText : public Text {
    public:
        explicit FreeTypeText(const FontPtr &font_) : Text(font_) {}
        ~FreeTypeText() override = default;

    private:
        bool Init(const TextDesc &desc) override;
        void Reset() override;
        void AddText(const std::string &text, const Vector2& pos, const TextInfo &info) override;
        static TextFlags ValidateFlags(const TextFlags &flags) ;

        std::unordered_map<BatchKey, TextBatchPtr> batches;
    };

} // namespace sky
