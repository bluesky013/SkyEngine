//
// Created for volume management system
//

#pragma once

#include <cstdint>
#include <core/shapes/AABB.h>
#include <core/shapes/Base.h>
#include <core/shapes/Frustum.h>
#include <core/shapes/Shapes.h>

namespace sky {

    class BoundingVolume {
    public:
        enum class Type : uint8_t {
            NONE,
            BOX,
            SPHERE
        };

        BoundingVolume() = default;
        explicit BoundingVolume(const AABB &aabb) : type(Type::BOX), aabb(aabb) {}
        explicit BoundingVolume(const Sphere &sphere) : type(Type::SPHERE), sphere(sphere) {}

        Type GetType() const { return type; }

        const AABB &GetAABB() const { return aabb; }
        const Sphere &GetSphere() const { return sphere; }

        void SetAABB(const AABB &box)
        {
            type = Type::BOX;
            aabb = box;
        }

        void SetSphere(const Sphere &sph)
        {
            type = Type::SPHERE;
            sphere = sph;
        }

        AABB ToAABB() const
        {
            switch (type) {
            case Type::BOX:
                return aabb;
            case Type::SPHERE:
                return AABB{sphere.center - Vector3(sphere.radius),
                            sphere.center + Vector3(sphere.radius)};
            default:
                return {};
            }
        }

        bool IntersectsFrustum(const Frustum &frustum) const
        {
            switch (type) {
            case Type::BOX:
                return Intersection(aabb, frustum);
            case Type::SPHERE:
                return Intersection(sphere, frustum);
            default:
                return false;
            }
        }

        bool IntersectsAABB(const AABB &other) const
        {
            switch (type) {
            case Type::BOX:
                return Intersection(aabb, other);
            case Type::SPHERE:
                return Intersection(sphere, other);
            default:
                return false;
            }
        }

    private:
        Type type = Type::NONE;
        AABB aabb;
        Sphere sphere;
    };

} // namespace sky
