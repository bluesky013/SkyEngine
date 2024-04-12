//
// Created by Zach Lee on 2023/9/3.
//

#pragma once

#include <core/math/Vector3.h>

namespace sky {

    struct Line {
        Vector3 begin;
        Vector3 end;
    };

    struct Triangle {
        std::array<Vector3, 4> v;
    };

    struct Quad {
        std::array<Vector3, 4> v;
    };

    struct Plane {
        Vector3 normal = VEC3_Y;
        float distance = 0;
    };

} // namespace sky