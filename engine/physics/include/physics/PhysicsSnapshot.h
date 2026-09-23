//
// Created on 2026/09/23.
//

#pragma once

#include <physics/PhysicsObjectId.h>

#include <core/math/Transform.h>
#include <core/math/Vector3.h>

#include <cstdint>
#include <vector>

namespace sky::phy {

    struct PhysicsBodyState {
        PhysicsObjectId id = INVALID_PHYSICS_OBJECT_ID;

        Transform transform;
        Vector3   linearVelocity  = VEC3_ZERO;
        Vector3   angularVelocity = VEC3_ZERO;
        bool      asleep          = false;
    };

    enum class PhysicsSnapshotScope : uint8_t {
        DynamicKinematic = 0,
        Full
    };

    // Backend-neutral world state addressed by stable handles. Re-simulation from a snapshot is
    // reserved for the future Exact mode; the delivered Fast mode restores snapshots directly.
    struct PhysicsWorldState {
        uint64_t frame = 0;
        PhysicsSnapshotScope scope = PhysicsSnapshotScope::DynamicKinematic;
        std::vector<PhysicsBodyState> bodies;
    };

} // namespace sky::phy
