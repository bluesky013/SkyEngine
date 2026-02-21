//
// Created by blues on 2026/2/19.
//

#pragma once

#include <core/math/Vector3.h>
#include <core/math/Matrix4.h>
#include <core/math/MathUtil.h>
#include <core/shapes/AABB.h>
#include <cmath>

namespace sky {

    struct BoundingBoxSphere {
        Vector3 center;
        Vector3 extent;
        float radius;

        FORCEINLINE constexpr BoundingBoxSphere()
            : center(VEC3_ZERO), extent(VEC3_ZERO), radius(0.f) {}

        FORCEINLINE constexpr BoundingBoxSphere(const Vector3 &center_, const Vector3 &extent_, float radius_)
            : center(center_), extent(extent_), radius(radius_) {}

        FORCEINLINE BoundingBoxSphere(const Vector3 &center_, const Vector3 &extent_)
            : center(center_), extent(extent_), radius(extent_.Length()) {}

        FORCEINLINE explicit BoundingBoxSphere(const AABB &aabb)
            : center((aabb.min + aabb.max) * 0.5f)
            , extent((aabb.max - aabb.min) * 0.5f)
            , radius(extent.Length()) {}

        FORCEINLINE AABB ToAABB() const { return AABB(center - extent, center + extent); }

        FORCEINLINE Vector3 Min() const { return center - extent; }
        FORCEINLINE Vector3 Max() const { return center + extent; }

        FORCEINLINE bool Contains(const Vector3 &point) const
        {
            return std::abs(point.x - center.x) <= extent.x &&
                   std::abs(point.y - center.y) <= extent.y &&
                   std::abs(point.z - center.z) <= extent.z;
        }

        FORCEINLINE bool Intersects(const BoundingBoxSphere &other) const
        {
            return std::abs(center.x - other.center.x) <= (extent.x + other.extent.x) &&
                   std::abs(center.y - other.center.y) <= (extent.y + other.extent.y) &&
                   std::abs(center.z - other.center.z) <= (extent.z + other.extent.z);
        }

        static FORCEINLINE BoundingBoxSphere FromMinMax(const Vector3 &min, const Vector3 &max)
        {
            Vector3 c = (min + max) * 0.5f;
            Vector3 e = (max - min) * 0.5f;
            return BoundingBoxSphere(c, e);
        }

        static FORCEINLINE BoundingBoxSphere Merge(const BoundingBoxSphere &a, const BoundingBoxSphere &b)
        {
            Vector3 minA = a.Min();
            Vector3 maxA = a.Max();
            Vector3 minB = b.Min();
            Vector3 maxB = b.Max();

            Vector3 newMin(std::fmin(minA.x, minB.x), std::fmin(minA.y, minB.y), std::fmin(minA.z, minB.z));
            Vector3 newMax(std::fmax(maxA.x, maxB.x), std::fmax(maxA.y, maxB.y), std::fmax(maxA.z, maxB.z));
            return FromMinMax(newMin, newMax);
        }

        FORCEINLINE BoundingBoxSphere Expanded(float amount) const
        {
            return BoundingBoxSphere(center, extent + Vector3(amount));
        }

        FORCEINLINE float DistanceTo(const Vector3 &point) const
        {
            float dx = std::fmax(std::abs(point.x - center.x) - extent.x, 0.f);
            float dy = std::fmax(std::abs(point.y - center.y) - extent.y, 0.f);
            float dz = std::fmax(std::abs(point.z - center.z) - extent.z, 0.f);
            return std::sqrt(dx * dx + dy * dy + dz * dz);
        }

        static FORCEINLINE BoundingBoxSphere Transform(const BoundingBoxSphere &bounds, const Matrix4 &matrix)
        {
            AABB aabb = AABB::Transform(bounds.ToAABB(), matrix);

            Vector3 axisX = ToVec3(matrix.m[0]);
            Vector3 axisY = ToVec3(matrix.m[1]);
            Vector3 axisZ = ToVec3(matrix.m[2]);

            float maxScale = std::fmax(std::fmax(axisX.Length(), axisY.Length()), axisZ.Length());

            BoundingBoxSphere result(aabb);
            result.radius = std::fmin(bounds.radius * maxScale, result.extent.Length());
            return result;
        }
    };

} // namespace sky
