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
}