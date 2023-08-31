//
// Created by Zach Lee on 2023/8/29.
//

#include <core/shapes/Shapes.h>
#include <core/math/MathUtil.h>

namespace sky {

    bool Intersection(const AABB &lhs, const AABB &rhs)
    {
        return (lhs.min.x <= rhs.max.x && lhs.max.x >= rhs.min.x) &&
               (lhs.min.y <= rhs.max.y && lhs.max.y >= rhs.min.y) &&
               (lhs.min.z <= rhs.max.z && lhs.max.z >= rhs.min.z);
    }

    std::pair<bool, int> Intersection(const AABB &aabb, const Plane &plane)
    {
        Vector3 center = (aabb.min + aabb.max) * 0.5f;
        Vector3 ext = aabb.max - center;

        float r = ext.x * std::abs(plane.normal.x) +
                  ext.y * std::abs(plane.normal.y) +
                  ext.z * std::abs(plane.normal.z);

        float s = plane.normal.Dot(center) - plane.distance;
        return {
            std::abs(s) <= r,
            s < -r ? -1 : (s > r ? 1 : 0)
        };
    }

    bool Intersection(const AABB &aabb, const Frustum &frustum)
    {
        return std::all_of(frustum.planes.begin(), frustum.planes.end(),
                           [&aabb](const Plane &plane) {
                               return Intersection(aabb, plane).second <= 0;
                           });
    }
} // namespace sky
