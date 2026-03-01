//
// Created by Zach Lee on 2026/2/23.
//

#pragma once

#include <cstdint>
#include <core/hash/Hash.h>

namespace sky {

    /**
     * @brief Grid coordinates for a PVS cell
     */
    struct PVSCellCoord {
        int32_t x = 0;
        int32_t y = 0;
        int32_t z = 0;

        bool operator == (const PVSCellCoord &other) const {
            return x == other.x && y == other.y && z == other.z;
        }

        bool operator != (const PVSCellCoord &other) const {
            return !(*this == other);
        }
    };

    /**
     * @brief Hash function for PVSCellCoord
     */
    struct PVSCellCoordHash {
        size_t operator()(const PVSCellCoord &coord) const
        {
            size_t h1 = std::hash<int32_t>{}(coord.x);
            size_t h2 = std::hash<int32_t>{}(coord.y);
            size_t h3 = std::hash<int32_t>{}(coord.z);
            return h1 ^ (h2 << 1) ^ (h3 << 2);
        }

        static uint32_t Hash32(const PVSCellCoord &coord)
        {
            uint32_t res = 0;
            HashCombine32(res, static_cast<uint32_t>(coord.x));
            HashCombine32(res, static_cast<uint32_t>(coord.y));
            HashCombine32(res, static_cast<uint32_t>(coord.z));
            return res;
        }
    };


    /**
     * @brief Grid coordinates for a sector in the world
     */
    struct PVSSectorCoord {
        int32_t x = 0;
        int32_t y = 0;

        bool operator == (const PVSSectorCoord &other) const {
            return x == other.x && y == other.y;
        }

        bool operator != (const PVSSectorCoord &other) const {
            return !(*this == other);
        }
    };

    /**
     * @brief Hash function for PVSSectorCoord
     */
    struct PVSSectorCoordHash {
        size_t operator()(const PVSSectorCoord &coord) const {
            size_t h1 = std::hash<int32_t>{}(coord.x);
            size_t h2 = std::hash<int32_t>{}(coord.y);
            return h1 ^ (h2 << 1);
        }

        static uint32_t Hash32(const PVSSectorCoord &coord)
        {
            uint32_t res = 0;
            HashCombine32(res, static_cast<uint32_t>(coord.x));
            HashCombine32(res, static_cast<uint32_t>(coord.y));
            return res;
        }
    };

} // namespace sky