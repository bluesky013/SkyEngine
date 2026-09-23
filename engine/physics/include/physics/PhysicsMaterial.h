//
// Created by blues on 2024/10/6.
//

#pragma once

#include <physics/PhysicsObjectId.h>

#include <cstdint>

namespace sky::phy {

    enum class PhysicsCombineMode : uint8_t {
        Average = 0,
        Min,
        Multiply,
        Max
    };

    // Backend-neutral material values.
    struct PhysicsMaterialData {
        float staticFriction  = 0.5f;
        float dynamicFriction = 0.5f;
        float restitution     = 0.f;

        PhysicsCombineMode frictionCombine    = PhysicsCombineMode::Average;
        PhysicsCombineMode restitutionCombine = PhysicsCombineMode::Average;

        float linearDamping  = 0.f;
        float angularDamping = 0.f;
    };

    inline PhysicsMaterialData GetDefaultPhysicsMaterial()
    {
        return PhysicsMaterialData{};
    }

    // World-scoped material handle returned by the world when a material is created.
    using PhysicsMaterialId = PhysicsObjectId;

} // namespace sky::phy
