//
// Created by Zach Lee on 2022/9/10.
//

#pragma once

#include <cstdint>
#include <cmath>
#include <algorithm>

namespace sky {
    struct Color;
    struct ColorRGB;

    struct Color {
        union {
            float v[4];
            struct {
                float r;
                float g;
                float b;
                float a;
            };
        };

        Color();
        Color(float r_, float g_, float b_, float a_);
        explicit Color(const ColorRGB& rgb);
    };
    using ColorRGBA = Color;

    struct ColorRGB {
        union {
            float v[3];
            struct {
                float r;
                float g;
                float b;
            };
        };

        ColorRGB();
        ColorRGB(float r_, float g_, float b_);
        explicit ColorRGB(const Color& rgba);
    };

    struct Color32 {
        uint32_t color;

        constexpr explicit Color32(uint32_t clr)
        {
            color = clr;
        }

        constexpr Color32(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
        {
            color = (static_cast<uint32_t>(r) << 0) |
                    (static_cast<uint32_t>(g) << 8) |
                    (static_cast<uint32_t>(b) << 16) |
                    (static_cast<uint32_t>(a) << 24);
        }
    };

    struct UColor {
        union {
            uint16_t v[4];
            struct {
                uint16_t r;
                uint16_t g;
                uint16_t b;
                uint16_t a;
            };
        };

        UColor();
        UColor(uint16_t r_, uint16_t g_, uint16_t b_, uint16_t a_);
    };

    inline float U8ToF32(uint8_t in) { return static_cast<float>(in) / 255.f; }
    inline float U16ToF32(uint16_t in) { return static_cast<float>(in) / 65535.f; }

    inline uint8_t F32ToU8(float in) { return static_cast<uint8_t>(std::round(std::clamp(in, 0.f, 1.f) * 255.f)); }
    inline uint16_t F32ToU16(float in) { return static_cast<uint16_t>(std::round(std::clamp(in, 0.f, 1.f) * 65535.f)); }
}
