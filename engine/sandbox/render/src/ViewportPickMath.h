//
// Created on 2026/10/23.
//

#pragma once

#include <algorithm>
#include <cmath>
#include <core/math/MathUtil.h>
#include <core/math/Matrix4.h>
#include <core/shapes/AABB.h>

namespace sky::editor::pick {
    inline bool Finite(Vector3 v)
    {
        return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
    }
    inline Vector3 TransformPoint(const Matrix4 &m, Vector3 v, float w)
    {
        const auto p = m * Vector4{v.x, v.y, v.z, w};
        return {p.x, p.y, p.z};
    }
    inline bool ValidBounds(const AABB &box)
    {
        return Finite(box.min) && Finite(box.max) && box.min.x <= box.max.x && box.min.y <= box.max.y && box.min.z <= box.max.z;
    }
    inline AABB Merge(const AABB &a, const AABB &b)
    {
        return {Min(a.min, b.min), Max(a.max, b.max)};
    }
    inline double Area(const AABB &box)
    {
        const double x = double(box.max.x) - box.min.x, y = double(box.max.y) - box.min.y, z = double(box.max.z) - box.min.z;
        return x * y + y * z + z * x;
    }
    inline bool Bounds(const AABB &box, Vector3 origin, Vector3 dir, float nearDistance, float farDistance, float &entry)
    {
        if (!ValidBounds(box))
            return true;
        double lo = nearDistance, hi = farDistance;
        for (uint32_t axis = 0; axis < 3; ++axis) {
            if (dir[axis] == 0) {
                if (origin[axis] < box.min[axis] || origin[axis] > box.max[axis])
                    return false;
                continue;
            }
            const double a = (double(box.min[axis]) - origin[axis]) / dir[axis];
            const double b = (double(box.max[axis]) - origin[axis]) / dir[axis];
            lo             = std::max(lo, std::min(a, b));
            hi             = std::min(hi, std::max(a, b));
            if (lo > hi)
                return false;
        }
        entry = float(lo);
        return true;
    }
} // namespace sky::editor::pick
