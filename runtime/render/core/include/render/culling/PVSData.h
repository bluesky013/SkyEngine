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
         * @brief Get the cell ID for a world position
         * @param position World position
         * @return Cell ID or INVALID_PVS_CELL if outside bounds
         */
        PVSCellID GetCellID(const Vector3 &position) const;

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

    private:
        PVSConfig config;
        PVSCellCoord gridDimensions;

        std::vector<PVSCell> cells;
        std::vector<PVSBitSet> visibilityData;  // One bitset per cell

        // Empty bitset for invalid queries
        PVSBitSet emptyBitSet;
    };

} // namespace sky
