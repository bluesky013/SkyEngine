//
// Created by blues on 2024/9/1.
//

#pragma once

#include <core/math/Transform.h>
#include <core/math/Vector3.h>

namespace sky::phy {

    // Minimal kinematic character controller contract (capsule-based, input-driven).
    class CharacterController {
    public:
        CharacterController() = default;
        virtual ~CharacterController() = default;

        virtual void      SetWorldTransform(const Transform &trans) {}
        virtual Transform GetWorldTransform() const { return Transform::GetIdentity(); }

        virtual void SetCapsule(float radius, float height) {}
        virtual void SetGravity(const Vector3 &gravity) {}
        virtual void SetStepHeight(float height) {}
        virtual void SetSlopeLimit(float degrees) {}

        // Applies a displacement for the next movement step; returns true when accepted.
        virtual bool Move(const Vector3 &displacement) { return false; }
        virtual bool IsGrounded() const { return false; }
    };

} // namespace sky::phy
