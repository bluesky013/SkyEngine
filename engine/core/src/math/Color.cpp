//
// Created by Zach Lee on 2023/4/1.
//

#include <core/math/Color.h>

#include <algorithm>
#include <cmath>

namespace sky {

    Color::Color() : Color(0, 0, 0, 0)
    {
    }

    Color::Color(float r_, float g_, float b_, float a_) : r(r_), g(g_), b(b_), a(a_)
    {
    }

    Color::Color(const ColorRGB& rgb) : Color(rgb.r, rgb.g, rgb.b, 1.f)
    {

    }

    ColorRGB::ColorRGB() : ColorRGB(0, 0, 0)
    {
    }

    ColorRGB::ColorRGB(float r_, float g_, float b_) : r(r_), g(g_), b(b_)
    {
    }

    ColorRGB::ColorRGB(const Color& rgba) : ColorRGB(rgba.r, rgba.g, rgba.b)
    {
    }

    UColor::UColor() : UColor(0, 0, 0, 0)
    {
    }

    UColor::UColor(uint16_t r_, uint16_t g_, uint16_t b_, uint16_t a_) : r(r_), g(g_), b(b_), a(a_)
    {
    }

    ColorHSV ToHSV(const Color &color)
    {
        const float mx = std::max(color.r, std::max(color.g, color.b));
        const float mn = std::min(color.r, std::min(color.g, color.b));
        const float d = mx - mn;

        ColorHSV out{0.0f, mx > 0.0f ? d / mx : 0.0f, mx};
        if (d <= 0.0f) {
            return out;
        }
        if (mx == color.r) {
            out.h = std::fmod((color.g - color.b) / d, 6.0f);
        } else if (mx == color.g) {
            out.h = (color.b - color.r) / d + 2.0f;
        } else {
            out.h = (color.r - color.g) / d + 4.0f;
        }
        out.h /= 6.0f;
        if (out.h < 0.0f) {
            out.h += 1.0f;
        }
        return out;
    }

    Color FromHSV(float h, float s, float v, float a)
    {
        h = h - std::floor(h);
        s = std::clamp(s, 0.0f, 1.0f);
        v = std::clamp(v, 0.0f, 1.0f);

        const float c = v * s;
        const float x = c * (1.0f - std::fabs(std::fmod(h * 6.0f, 2.0f) - 1.0f));
        const float m = v - c;
        float r = 0.0f;
        float g = 0.0f;
        float b = 0.0f;
        switch (static_cast<int>(h * 6.0f)) {
        case 0: r = c; g = x; break;
        case 1: r = x; g = c; break;
        case 2: g = c; b = x; break;
        case 3: g = x; b = c; break;
        case 4: r = x; b = c; break;
        default: r = c; b = x; break;
        }
        return Color(r + m, g + m, b + m, a);
    }
}