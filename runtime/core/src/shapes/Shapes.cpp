//
// Created by Zach Lee on 2023/8/29.
//

#include <core/shapes/Shapes.h>
#include <algorithm>
#include <cmath>
#include <limits>
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

    std::tuple<bool, float, float> Intersection(const Ray &ray, const AABB &aabb)
    {
        // Slab-based ray-AABB intersection
        float tMin = 0.0f;
        float tMax = std::numeric_limits<float>::max();

        // X axis
        if (std::abs(ray.dir.x) < 1e-6f) {
            // Ray parallel to X slab
            if (ray.origin.x < aabb.min.x || ray.origin.x > aabb.max.x) {
                return {false, 0.0f, 0.0f};
            }
        } else {
            float invD = 1.0f / ray.dir.x;
            float t1 = (aabb.min.x - ray.origin.x) * invD;
            float t2 = (aabb.max.x - ray.origin.x) * invD;
            if (t1 > t2) std::swap(t1, t2);
            tMin = std::max(tMin, t1);
            tMax = std::min(tMax, t2);
            if (tMin > tMax) return {false, 0.0f, 0.0f};
        }

        // Y axis
        if (std::abs(ray.dir.y) < 1e-6f) {
            if (ray.origin.y < aabb.min.y || ray.origin.y > aabb.max.y) {
                return {false, 0.0f, 0.0f};
            }
        } else {
            float invD = 1.0f / ray.dir.y;
            float t1 = (aabb.min.y - ray.origin.y) * invD;
            float t2 = (aabb.max.y - ray.origin.y) * invD;
            if (t1 > t2) std::swap(t1, t2);
            tMin = std::max(tMin, t1);
            tMax = std::min(tMax, t2);
            if (tMin > tMax) return {false, 0.0f, 0.0f};
        }

        // Z axis
        if (std::abs(ray.dir.z) < 1e-6f) {
            if (ray.origin.z < aabb.min.z || ray.origin.z > aabb.max.z) {
                return {false, 0.0f, 0.0f};
            }
        } else {
            float invD = 1.0f / ray.dir.z;
            float t1 = (aabb.min.z - ray.origin.z) * invD;
            float t2 = (aabb.max.z - ray.origin.z) * invD;
            if (t1 > t2) std::swap(t1, t2);
            tMin = std::max(tMin, t1);
            tMax = std::min(tMax, t2);
            if (tMin > tMax) return {false, 0.0f, 0.0f};
        }

        return {true, tMin, tMax};
    }

    bool IntersectionTest(const Ray &ray, const AABB &aabb)
    {
        return std::get<0>(Intersection(ray, aabb));
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
