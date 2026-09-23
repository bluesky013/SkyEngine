//
// Created on 2026/09/23.
//

#pragma once

#include <physics/PhysicsObjectId.h>

#include <core/math/Vector3.h>

#include <cstdint>
#include <string>

namespace sky::phy {

    enum class ConstraintType : uint8_t {
        Fixed = 0,
        Hinge,
        Slider,
        Distance,
        Generic6DOF
    };

    struct ConstraintLimit {
        bool  limited = false;
        float lower   = 0.f;
        float upper   = 0.f;
    };

    struct ConstraintMotor {
        bool  enabled        = false;
        float targetVelocity = 0.f;
        float targetPosition = 0.f;
        float maxForce       = 0.f;
        float maxTorque      = 0.f;
    };

    struct ConstraintDesc {
        ConstraintType type = ConstraintType::Fixed;

        PhysicsObjectId bodyA = INVALID_PHYSICS_OBJECT_ID;
        PhysicsObjectId bodyB = INVALID_PHYSICS_OBJECT_ID;

        Vector3 pivotA = VEC3_ZERO;
        Vector3 pivotB = VEC3_ZERO;

        Vector3 axisA = Vector3(0, 1, 0);
        Vector3 axisB = Vector3(0, 1, 0);

        float distance = 0.f;

        ConstraintLimit linearX;
        ConstraintLimit linearY;
        ConstraintLimit linearZ;
        ConstraintLimit angularX;
        ConstraintLimit angularY;
        ConstraintLimit angularZ;

        ConstraintMotor linearMotor;
        ConstraintMotor angularMotor;

        bool collideConnected = false;
    };

    bool ValidateConstraintDesc(const ConstraintDesc &desc, std::string *why = nullptr);

} // namespace sky::phy
