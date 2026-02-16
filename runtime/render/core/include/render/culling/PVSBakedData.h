//
// Created by SkyEngine on 2024/02/16.
//

#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <render/culling/PVSTypes.h>
#include <render/culling/PVSBitSet.h>
#include <core/shapes/AABB.h>

namespace sky {

    class BinaryInputArchive;
    class BinaryOutputArchive;

    /**
     * @brief Serializable PVS baked data structure
     * 
     * Contains all pre-computed visibility data that can be saved to disk
     * and loaded at runtime.
     */
    struct PVSBakedData {
        // Version for compatibility
        static constexpr uint32_t VERSION = 1;

        // Configuration used for baking
        PVSConfig config;
        
        // Grid dimensions
        PVSCellCoord gridDimensions;
        
        // Number of objects
        uint32_t numObjects = 0;
        
        // Cell data (bounds, centers)
        std::vector<PVSCell> cells;
        
        // Visibility bitsets (one per cell)
        // Stored as raw uint64_t arrays for efficient serialization
        std::vector<std::vector<uint64_t>> visibilityData;
        
        // Object UUIDs or names for mapping (optional)
        std::vector<std::string> objectNames;

        /**
         * @brief Save the baked data to a binary archive
         */
        void Save(BinaryOutputArchive &archive) const;

        /**
         * @brief Load baked data from a binary archive
         */
        void Load(BinaryInputArchive &archive);

        /**
         * @brief Calculate the total memory size of the baked data
         */
        size_t GetMemorySize() const;

        /**
         * @brief Get statistics about the baked data
         */
        struct Statistics {
            uint32_t totalCells = 0;
            uint32_t totalObjects = 0;
            uint32_t totalVisiblePairs = 0;  // Total cell-object visibility pairs
            float averageVisibleObjects = 0.0f;
            float compressionRatio = 0.0f;
            size_t rawDataSize = 0;
        };
        Statistics GetStatistics() const;
    };

} // namespace sky
