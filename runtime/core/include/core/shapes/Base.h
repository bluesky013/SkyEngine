//
// Created by Zach Lee on 2023/9/3.
//

#pragma once

#include <core/math/Vector2.h>
#include <core/math/Vector3.h>
#include <array>

namespace sky {

    struct Line {
        Vector3 begin;
        Vector3 end;
    };

    struct Triangle {
        std::array<Vector3, 3> v;
    };

    struct Quad {
        std::array<Vector3, 4> v;
    };

    struct Rect {
        Vector2 offset = VEC2_ZERO;
        Vector2 ext    = VEC2_ZERO;
    };

    struct Plane {
        Vector3 normal = VEC3_Y;
        float distance = 0;
    };

    struct Sphere {
        Vector3 center = VEC3_ZERO;
        float radius = 1.f;
    };

    struct Capsule {
        float halfHeight = 1.f;
        float radius = 1.f;
    };

} // namespace sky