//
// Created by Zach Lee on 2022/9/10.
//

#pragma once

#include <cstdint>

namespace sky {

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
    };

    struct Color32 {
        uint32_t color;

        constexpr Color32(uint32_t clr)
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
}
