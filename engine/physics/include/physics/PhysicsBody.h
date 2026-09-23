//
// Created on 2026/09/23.
//

#pragma once

#include <physics/PhysicsFilter.h>
#include <physics/PhysicsMaterial.h>
#include <physics/PhysicsObjectId.h>
#include <physics/PhysicsShapes.h>

#include <core/math/Transform.h>
#include <core/math/Vector3.h>

#include <string>

namespace sky::phy {

    enum class BodyKind : uint8_t {
        Static = 0,
        Dynamic,
        Kinematic
    };

    // Backend-neutral body description. The world owns the created body; the caller keeps only the
    // returned PhysicsObjectId.
    struct PhysicsBodyDesc {
        BodyKind  kind = BodyKind::Static;
        ShapeDesc shape;

        float mass = 1.f;
        // Zero on an axis means "let the backend compute this component from the shape".
        Vector3 inertia = VEC3_ZERO;

        Vector3 linearDamping  = VEC3_ZERO;
        Vector3 angularDamping = VEC3_ZERO;

        PhysicsMaterialId material = INVALID_PHYSICS_OBJECT_ID;
        CollisionFilter   filter;

        bool enableCCD      = false;
        bool gravityEnabled = true;
        bool allowSleep     = true;
        bool startsAsleep   = false;

        // Trigger volumes report enter/exit events without applying collision response.
        bool isTrigger = false;

        Transform transform;
    };

    bool ValidateBodyDesc(const PhysicsBodyDesc &desc, std::string *why = nullptr);

} // namespace sky::phy
