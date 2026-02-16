//
// Created by SkyEngine on 2024/02/15.
//

#pragma once

#include <cstdint>
#include <vector>
#include <core/math/Vector3.h>
#include <core/shapes/AABB.h>

namespace sky {

    /**
     * @brief Unique identifier for a PVS cell
     */
    using PVSCellID = uint32_t;
    static constexpr PVSCellID INVALID_PVS_CELL = ~(0U);

    /**
     * @brief Unique identifier for an object in the PVS system
     */
    using PVSObjectID = uint32_t;
    static constexpr PVSObjectID INVALID_PVS_OBJECT = ~(0U);

    /**
     * @brief Configuration for PVS grid generation
     */
    struct PVSConfig {
        AABB worldBounds;           // World space bounds for the entire PVS system
        Vector3 cellSize;           // Size of each cell in world units
        uint32_t maxObjects = 4096; // Maximum number of objects to track
        bool enablePortals = false; // Whether to use portal-based visibility
    };

    /**
     * @brief Represents a single cell in the PVS grid
     */
    struct PVSCell {
        PVSCellID id = INVALID_PVS_CELL;
        AABB bounds;
        Vector3 center;
    };

    /**
     * @brief Grid coordinates for a PVS cell
     */
    struct PVSCellCoord {
        int32_t x = 0;
        int32_t y = 0;
        int32_t z = 0;

        bool operator==(const PVSCellCoord &other) const {
            return x == other.x && y == other.y && z == other.z;
        }

        bool operator!=(const PVSCellCoord &other) const {
            return !(*this == other);
        }
    };

    /**
     * @brief Hash function for PVSCellCoord
     */
    struct PVSCellCoordHash {
        size_t operator()(const PVSCellCoord &coord) const {
            size_t h1 = std::hash<int32_t>{}(coord.x);
            size_t h2 = std::hash<int32_t>{}(coord.y);
            size_t h3 = std::hash<int32_t>{}(coord.z);
            return h1 ^ (h2 << 1) ^ (h3 << 2);
        }
    };

} // namespace sky
