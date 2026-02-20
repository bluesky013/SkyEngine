//
// Created by SkyEngine on 2024/02/20.
//

#pragma once

#include <cstdint>
#include <functional>
#include <render/culling/PVSTypes.h>
#include <render/culling/PVSBakedData.h>

namespace sky {

    /**
     * @brief Unique identifier for a world sector
     * 
     * A sector is a large chunk of the world that contains multiple PVS cells.
     * Sectors are streamed in/out based on player position.
     */
    using PVSSectorID = uint32_t;
    static constexpr PVSSectorID INVALID_PVS_SECTOR = ~(0U);

    /**
     * @brief Grid coordinates for a sector in the world
     */
    struct PVSSectorCoord {
        int32_t x = 0;
        int32_t y = 0;
        int32_t z = 0;

        bool operator==(const PVSSectorCoord &other) const {
            return x == other.x && y == other.y && z == other.z;
        }

        bool operator!=(const PVSSectorCoord &other) const {
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
            size_t h3 = std::hash<int32_t>{}(coord.z);
            return h1 ^ (h2 << 1) ^ (h3 << 2);
        }
    };

    /**
     * @brief Configuration for streaming PVS sectors
     */
    struct PVSStreamingConfig {
        // World configuration
        AABB worldBounds;                   // Total world bounds (can be very large)
        Vector3 sectorSize;                 // Size of each sector in world units
        Vector3 cellSize;                   // Size of each cell within sectors
        
        // Streaming configuration
        float loadRadius = 200.0f;          // Distance to load sectors
        float unloadRadius = 300.0f;        // Distance to unload sectors (should be > loadRadius)
        uint32_t maxLoadedSectors = 16;     // Maximum sectors in memory
        bool preloadNeighbors = true;       // Preload adjacent sectors
        
        // Object configuration
        uint32_t maxObjectsPerSector = 1024; // Max objects per sector
    };

    /**
     * @brief State of a streaming sector
     */
    enum class PVSSectorState {
        UNLOADED,       // Not in memory
        LOADING,        // Being loaded asynchronously
        LOADED,         // Fully loaded and ready
        UNLOADING       // Being unloaded
    };

    /**
     * @brief Information about a streaming sector
     */
    struct PVSSectorInfo {
        PVSSectorID id = INVALID_PVS_SECTOR;
        PVSSectorCoord coord;
        AABB bounds;
        Vector3 center;
        PVSSectorState state = PVSSectorState::UNLOADED;
        
        // Usage tracking for LRU
        uint64_t lastUsedFrame = 0;
        float distanceToViewer = 0.0f;
    };

    /**
     * @brief Baked data for a single sector
     * 
     * This is what gets serialized to disk per sector file.
     */
    struct PVSSectorBakedData {
        static constexpr uint32_t VERSION = 1;
        
        // Sector identification
        PVSSectorCoord coord;
        AABB bounds;
        
        // PVS data for this sector
        PVSBakedData pvsData;
        
        // Object ID mapping: local ID -> global object name
        // This allows objects to be identified across sector boundaries
        std::vector<std::string> objectNames;
        
        /**
         * @brief Save sector data to binary archive
         */
        void Save(class BinaryOutputArchive &archive) const;
        
        /**
         * @brief Load sector data from binary archive
         */
        void Load(class BinaryInputArchive &archive);
        
        /**
         * @brief Get memory size of sector data
         */
        size_t GetMemorySize() const;
    };

    /**
     * @brief Callback types for async sector operations
     */
    using PVSSectorLoadCallback = std::function<void(PVSSectorID sectorID, bool success)>;
    using PVSSectorUnloadCallback = std::function<void(PVSSectorID sectorID)>;
    using PVSSectorDataProvider = std::function<bool(PVSSectorCoord coord, PVSSectorBakedData &outData)>;

} // namespace sky
