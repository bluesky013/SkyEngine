//
// Created by blues on 2024/9/11.
//

#pragma once

#include <render/text/Font.h>
#include <core/math/Color.h>
#include <core/math/Vector2.h>
#include <core/template/Flags.h>
#include <core/template/ReferenceObject.h>
#include <core/hash/Hash.h>
#include <xutility>
#include <cstdint>

namespace sky {

    struct TextDesc {
        uint32_t fontSize  = 10;
        uint32_t texWidth  = 512;
        uint32_t texHeight = 512;
    };

    enum class TextFlagBit : uint32_t {
        NONE    = 0x00,
        BOLD    = 0x01,
        ITALIC  = 0x02,
        SHADOW  = 0x03,
    };
    using TextFlags = Flags<TextFlagBit>;
    ENABLE_FLAG_BIT_OPERATOR(TextFlagBit)

    struct BatchKey {
        TextFlags flags;
        uint32_t texIndex;
    };

    struct TextInfo {
        Color     color;
        Color     shadowColor;
        TextFlags flags;
        uint32_t  shadowThickness = 1;
        float     scale = 1.f;
    };

    class Text : public RefObject {
    public:
        explicit Text(const FontPtr &font_) : font(font_) {} // NOLINT
        ~Text() override = default;

        virtual bool Init(const TextDesc &desc) = 0;

        virtual void Reset() = 0;
        virtual void AddText(const std::string &text, const Vector2& pos, const TextInfo &info) = 0;
    protected:
        FontPtr font;
    };
    using TextPtr = CounterPtr<Text>;

} // namespace sky

namespace std {

    template <>
    struct hash<sky::BatchKey> {
        size_t operator()(const sky::BatchKey &flags) const noexcept
        {
            uint32_t hash = flags.flags.value;
            sky::HashCombine32(hash, flags.texIndex);
            return hash;
        }
    };

    template <>
    struct equal_to<sky::BatchKey> {
        bool operator()(const sky::BatchKey &x, const sky::BatchKey &y) const noexcept
        {
            return x.texIndex == y.texIndex && x.flags == y.flags;
        }
    };

} // namespace std