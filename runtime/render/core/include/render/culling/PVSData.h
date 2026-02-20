//
// Created by SkyEngine on 2024/02/15.
//

#pragma once

#include <vector>
#include <unordered_map>
#include <render/culling/PVSTypes.h>
#include <render/culling/PVSBitSet.h>

namespace sky {

    /**
     * @brief Stores pre-computed visibility data for a 3D grid of cells
     * 
     * The PVSData structure divides the world into a uniform grid of cells.
     * For each cell, it stores a bitset indicating which objects are potentially
     * visible from that cell.
     * 
     * Usage:
     * 1. Configure the PVS system with world bounds and cell size
     * 2. Register objects to get object IDs
     * 3. Compute visibility for each cell (offline or at load time)
     * 4. At runtime, query visibility by position
     */
    class PVSData {
    public:
        PVSData() = default;
        ~PVSData() = default;

        /**
         * @brief Initialize the PVS grid with configuration
         * @param config Configuration specifying world bounds and cell size
         */
        void Initialize(const PVSConfig &config);

        /**
         * @brief Clear all PVS data
         */
        void Clear();

        /**
         * @brief Get the cell ID for a world position (optimized)
         * 
         * Uses pre-computed inverse cell sizes to avoid division.
         * 
         * @param position World position
         * @return Cell ID or INVALID_PVS_CELL if outside bounds
         */
        PVSCellID GetCellID(const Vector3 &position) const;

        /**
         * @brief Get the cell ID for a world position (fast inline version)
         * 
         * This version includes clamping to ensure valid cell indices even for
         * positions at the boundaries. Use IsInBounds() first if you need to
         * distinguish between valid and invalid positions.
         * 
         * @param position World position (ideally within bounds, clamped if not)
         * @return Cell ID (always valid due to clamping)
         */
        inline PVSCellID GetCellIDClamped(const Vector3 &position) const
        {
            int32_t x = static_cast<int32_t>((position.x - config.worldBounds.min.x) * invCellSize.x);
            int32_t y = static_cast<int32_t>((position.y - config.worldBounds.min.y) * invCellSize.y);
            int32_t z = static_cast<int32_t>((position.z - config.worldBounds.min.z) * invCellSize.z);
            
            // Clamp to valid range
            x = x < 0 ? 0 : (x >= gridDimensions.x ? gridDimensions.x - 1 : x);
            y = y < 0 ? 0 : (y >= gridDimensions.y ? gridDimensions.y - 1 : y);
            z = z < 0 ? 0 : (z >= gridDimensions.z ? gridDimensions.z - 1 : z);
            
            return static_cast<PVSCellID>(z * xyArea + y * gridDimensions.x + x);
        }

        /**
         * @brief Get the cell ID for a position known to be within bounds (fastest)
         * 
         * No bounds checking or clamping. Caller must ensure position is within
         * world bounds, otherwise behavior is undefined.
         * 
         * @param position World position (MUST be within world bounds)
         * @return Cell ID
         */
        inline PVSCellID GetCellIDFast(const Vector3 &position) const
        {
            // Direct calculation - position must be within bounds
            int32_t x = static_cast<int32_t>((position.x - config.worldBounds.min.x) * invCellSize.x);
            int32_t y = static_cast<int32_t>((position.y - config.worldBounds.min.y) * invCellSize.y);
            int32_t z = static_cast<int32_t>((position.z - config.worldBounds.min.z) * invCellSize.z);
            
            return static_cast<PVSCellID>(z * xyArea + y * gridDimensions.x + x);
        }

        /**
         * @brief Get the cell coordinates for a world position
         * @param position World position
         * @return Cell coordinates
         */
        PVSCellCoord GetCellCoord(const Vector3 &position) const;

        /**
         * @brief Get cell ID from coordinates
         * @param coord Cell coordinates
         * @return Cell ID or INVALID_PVS_CELL if outside bounds
         */
        PVSCellID GetCellIDFromCoord(const PVSCellCoord &coord) const;

        /**
         * @brief Get cell ID from coordinates (fast inline version)
         * @param coord Cell coordinates (must be valid)
         * @return Cell ID
         */
        inline PVSCellID GetCellIDFromCoordFast(const PVSCellCoord &coord) const
        {
            return static_cast<PVSCellID>(coord.z * xyArea + coord.y * gridDimensions.x + coord.x);
        }

        /**
         * @brief Get cell coordinates from cell ID
         * @param cellID Cell ID
         * @return Cell coordinates
         */
        PVSCellCoord GetCoordFromCellID(PVSCellID cellID) const;

        /**
         * @brief Get the cell information
         * @param cellID Cell ID
         * @return Cell information
         */
        const PVSCell& GetCell(PVSCellID cellID) const;

        /**
         * @brief Set an object as visible from a cell
         * @param cellID Cell ID
         * @param objectID Object ID
         */
        void SetVisible(PVSCellID cellID, PVSObjectID objectID);

        /**
         * @brief Clear visibility for an object from a cell
         * @param cellID Cell ID
         * @param objectID Object ID
         */
        void ClearVisible(PVSCellID cellID, PVSObjectID objectID);

        /**
         * @brief Check if an object is visible from a cell
         * @param cellID Cell ID
         * @param objectID Object ID
         * @return True if the object is potentially visible from the cell
         */
        bool IsVisible(PVSCellID cellID, PVSObjectID objectID) const;

        /**
         * @brief Get the visibility bitset for a cell
         * @param cellID Cell ID
         * @return Reference to the visibility bitset
         */
        const PVSBitSet& GetVisibilitySet(PVSCellID cellID) const;

        /**
         * @brief Get a mutable visibility bitset for a cell
         * @param cellID Cell ID
         * @return Reference to the visibility bitset
         */
        PVSBitSet& GetMutableVisibilitySet(PVSCellID cellID);

        /**
         * @brief Set all objects visible from a cell
         * @param cellID Cell ID
         */
        void SetAllVisible(PVSCellID cellID);

        /**
         * @brief Clear all visibility from a cell
         * @param cellID Cell ID
         */
        void ClearAllVisible(PVSCellID cellID);

        /**
         * @brief Get the number of cells in the grid
         */
        uint32_t GetCellCount() const { return static_cast<uint32_t>(cells.size()); }

        /**
         * @brief Get grid dimensions
         */
        const PVSCellCoord& GetGridDimensions() const { return gridDimensions; }

        /**
         * @brief Get the configuration
         */
        const PVSConfig& GetConfig() const { return config; }

        /**
         * @brief Check if a cell ID is valid
         */
        bool IsValidCell(PVSCellID cellID) const { return cellID < GetCellCount(); }

        /**
         * @brief Check if a position is within the world bounds
         */
        inline bool IsInBounds(const Vector3 &position) const
        {
            return position.x >= config.worldBounds.min.x && position.x < config.worldBounds.max.x &&
                   position.y >= config.worldBounds.min.y && position.y < config.worldBounds.max.y &&
                   position.z >= config.worldBounds.min.z && position.z < config.worldBounds.max.z;
        }

        /**
         * @brief Load PVS data from baked data
         * @param bakedData Pre-computed visibility data
         */
        void LoadFromBakedData(const struct PVSBakedData &bakedData);

        /**
         * @brief Export to baked data format
         * @param outBakedData Output baked data structure
         */
        void ExportToBakedData(struct PVSBakedData &outBakedData) const;

    private:
        PVSConfig config;
        PVSCellCoord gridDimensions;
        
        // Pre-computed values for fast cell lookup
        Vector3 invCellSize;  // 1.0 / cellSize for multiplication instead of division
        int32_t xyArea = 0;   // gridDimensions.x * gridDimensions.y

        std::vector<PVSCell> cells;
        std::vector<PVSBitSet> visibilityData;  // One bitset per cell

        // Empty bitset for invalid queries
        PVSBitSet emptyBitSet;
    };

} // namespace sky
