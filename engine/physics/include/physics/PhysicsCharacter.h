//
// Created on 2026/09/23.
//

#pragma once

#include <physics/PhysicsFilter.h>
#include <physics/PhysicsObjectId.h>

#include <core/math/Transform.h>
#include <core/math/Vector3.h>

#include <cstdint>
#include <string>

namespace sky::phy {

    struct CharacterDesc {
        float   radius     = 0.4f;
        float   height     = 1.4f; // cylindrical section height, excludes the two caps
        float   stepHeight = 0.4f;
        float   slopeLimit = 45.f; // walkable slope limit in degrees
        float   gravity    = 9.81f;
        Vector3 up         = Vector3(0, 1, 0);

        CollisionFilter filter;
        Transform       transform;
    };

    enum class CharacterMoveResult : uint8_t {
        Applied = 0,
        Rejected,
        NotAttached
    };

    // Character runtime state read back from the world.
    struct CharacterState {
        bool      grounded = false;
        Transform transform;
    };

    bool ValidateCharacterDesc(const CharacterDesc &desc, std::string *why = nullptr);

} // namespace sky::phy
