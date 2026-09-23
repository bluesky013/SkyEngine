//
// Created on 2026/09/22.
//

#pragma once

#include <physics/PhysicsObjectId.h>

#include <core/math/Vector3.h>

#include <vector>

namespace sky::phy {

    // Handle-based results: the hit object is addressed by a stable id for every object kind, with no
    // backend pointer. Ordered by ascending distance, then by handle.
    struct PhysicsQueryHit {
        PhysicsObjectId object = INVALID_PHYSICS_OBJECT_ID;

        Vector3 position = VEC3_ZERO;
        Vector3 normal   = VEC3_ZERO;
        float   distance = 0.f;
    };

    struct PhysicsQueryOverlap {
        PhysicsObjectId object = INVALID_PHYSICS_OBJECT_ID;
    };

} // namespace sky::phy
