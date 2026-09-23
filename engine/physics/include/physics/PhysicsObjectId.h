//
// Created on 2026/09/23.
//

#pragma once

#include <cstdint>
#include <functional>

namespace sky::phy {

    // Stable handle to a physics world object (body, character, constraint). Scoped to one world.
    struct PhysicsObjectId {
        uint64_t index      = 0;
        uint32_t generation = 0;

        constexpr bool operator==(const PhysicsObjectId &rhs) const
        {
            return index == rhs.index && generation == rhs.generation;
        }

        constexpr bool operator!=(const PhysicsObjectId &rhs) const { return !(*this == rhs); }

        constexpr bool operator<(const PhysicsObjectId &rhs) const
        {
            return index != rhs.index ? index < rhs.index : generation < rhs.generation;
        }
    };

    // index 0 with generation 0 is reserved so that a default handle never resolves.
    inline constexpr PhysicsObjectId INVALID_PHYSICS_OBJECT_ID = {0, 0};

    constexpr bool IsValid(PhysicsObjectId id)
    {
        return id.index != 0 && id.generation != 0;
    }

    struct PhysicsObjectIdHash {
        size_t operator()(const PhysicsObjectId &id) const noexcept
        {
            return std::hash<uint64_t>()(id.index) ^ (std::hash<uint32_t>()(id.generation) << 1);
        }
    };

} // namespace sky::phy
