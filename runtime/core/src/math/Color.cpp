//
// Created by Zach Lee on 2023/4/1.
//

#include <core/math/Color.h>

namespace sky {

    Color::Color() : Color(0, 0, 0, 0)
    {
    }

    Color::Color(float r_, float g_, float b_, float a_) : r(r_), g(g_), b(b_), a(a_)
    {
    }

    UColor::UColor() : UColor(0, 0, 0, 0)
    {
    }

    UColor::UColor(uint16_t r_, uint16_t g_, uint16_t b_, uint16_t a_) : r(r_), g(g_), b(b_), a(a_)
    {
    }
}