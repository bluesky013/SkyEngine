//
// Created by Zach Lee on 2023/8/29.
//

#include <core/shapes/Shapes.h>
#include <algorithm>
#include <core/math/MathUtil.h>

namespace sky {

    bool Intersection(const AABB &lhs, const AABB &rhs)
    {
        if (lhs.min.x > rhs.max.x || lhs.max.x < rhs.min.x) {
            return false;
        }
        if (lhs.min.y > rhs.max.y || lhs.max.y < rhs.min.y) {
            return false;
        }
        if (lhs.min.z > rhs.max.z || lhs.max.z < rhs.min.z) {
            return false;
        }

        return true;
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

    std::pair<bool, int> Intersection(const Sphere &sphere, const Plane &plane)
    {
        float dist = plane.normal.Dot(sphere.center) - plane.distance;
        if (std::abs(dist) <= sphere.radius) {
            return {true, 0};
        }
        return {false, dist > 0.f ? 1 : -1};
    }

    bool Intersection(const Sphere &sphere, const AABB &aabb)
    {
        float sqDist = 0.f;
        for (int i = 0; i < 3; ++i) {
            float v = sphere.center[i];
            if (v < aabb.min[i]) {
                float d = aabb.min[i] - v;
                sqDist += d * d;
            }
            if (v > aabb.max[i]) {
                float d = v - aabb.max[i];
                sqDist += d * d;
            }
        }
        return sqDist <= sphere.radius * sphere.radius;
    }

    bool Intersection(const Sphere &sphere, const Frustum &frustum)
    {
        return std::all_of(frustum.planes.begin(), frustum.planes.end(),
                           [&sphere](const Plane &plane) {
                               return Intersection(sphere, plane).second <= 0;
                           });
    }


    Plane CreatePlaneByVertices(const Vector3 &v1, const Vector3 &v2, const Vector3 &v3)
    {
        const Vector3 d1 = v2 - v1;
        const Vector3 d2 = v3 - v1;

        Vector3 normal = d1.Cross(d2);
        normal.Normalize();
        return CreatePlaneByNormalAndVertex(normal, v1);
    }

    Plane CreatePlaneByNormalAndVertex(const Vector3 &normal, const Vector3 &pt)
    {
        return Plane{normal, normal.Dot(pt)};
    }

    float DistanceToPlane(const Vector3 &pt, const Plane &plane)
    {
        return plane.normal.Dot(pt) + plane.distance;
    }

    std::pair<bool, Vector3> CalculateInterSection(const Ray &ray, const Plane &plane)
    {
        float denom = plane.normal.Dot(ray.dir);
        if (abs(denom) > 1e-6) {
            float t = -(plane.normal.Dot(ray.origin) + plane.distance) / denom;
            return {true, ray.origin + ray.dir * t};
        }
        return {false, VEC3_ZERO};
    }
} // namespace sky
