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

        inline Vector3 GetCenter() const { return (min + max) / 2.f; }
        inline Vector3 GetExtent() const { return (max - min) / 2.f; }
        inline bool IsValid() const { return min.x <= max.x && min.y <= max.y && min.z <= max.z; }
        inline bool Contains(const Vector3 &point) const
        {
            return point.x >= min.x && point.x <= max.x &&
                   point.y >= min.y && point.y <= max.y &&
                   point.z >= min.z && point.z <= max.z;
        }
    };

    void Merge(const AABB &a, const AABB &b, AABB &out);

    static constexpr AABB AABB_MAX = AABB(Vector3(std::numeric_limits<float>::min()), Vector3(std::numeric_limits<float>::max()));
} // namespace sky
