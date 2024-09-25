//
// Created by Zach Lee on 2023/8/29.
//

#pragma once

#include <core/math/Vector3.h>
#include <core/math/Matrix4.h>
#include <limits>

namespace sky {

    struct AABB {
        Vector3 min;
        Vector3 max;

        inline constexpr AABB() : min(VEC3_ZERO), max(VEC3_ZERO) {}
        inline constexpr AABB(Vector3 min_, Vector3 max_) : min(min_), max(max_) {}

        static AABB Transform(const AABB& box, const Matrix4 &matrix);
    };

    void Merge(const AABB &a, const AABB &b, AABB &out);

    static constexpr AABB AABB_MAX = AABB(Vector3(std::numeric_limits<float>::min()), Vector3(std::numeric_limits<float>::max()));
} // namespace sky
