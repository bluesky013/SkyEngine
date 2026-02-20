//
// Created by SkyEngine on 2024/02/15.
//

#include <render/culling/PVSData.h>
#include <render/culling/PVSBakedData.h>
#include <algorithm>
#include <cmath>

namespace sky {

    void PVSData::Initialize(const PVSConfig &cfg)
    {
        config = cfg;

        // Calculate grid dimensions
        Vector3 worldSize = config.worldBounds.max - config.worldBounds.min;
        
        gridDimensions.x = static_cast<int32_t>(std::ceil(worldSize.x / config.cellSize.x));
        gridDimensions.y = static_cast<int32_t>(std::ceil(worldSize.y / config.cellSize.y));
        gridDimensions.z = static_cast<int32_t>(std::ceil(worldSize.z / config.cellSize.z));

        // Ensure at least one cell in each dimension
        gridDimensions.x = std::max(1, gridDimensions.x);
        gridDimensions.y = std::max(1, gridDimensions.y);
        gridDimensions.z = std::max(1, gridDimensions.z);

        // Pre-compute inverse cell sizes for fast lookup (multiplication instead of division)
        invCellSize.x = 1.0f / config.cellSize.x;
        invCellSize.y = 1.0f / config.cellSize.y;
        invCellSize.z = 1.0f / config.cellSize.z;
        
        // Pre-compute XY area for fast cell ID calculation
        xyArea = gridDimensions.x * gridDimensions.y;

        // Calculate total number of cells
        uint32_t totalCells = static_cast<uint32_t>(gridDimensions.x * gridDimensions.y * gridDimensions.z);

        // Initialize cells
        cells.resize(totalCells);
        visibilityData.resize(totalCells);

        for (int32_t z = 0; z < gridDimensions.z; ++z) {
            for (int32_t y = 0; y < gridDimensions.y; ++y) {
                for (int32_t x = 0; x < gridDimensions.x; ++x) {
                    PVSCellID cellID = GetCellIDFromCoord({x, y, z});
                    
                    // Calculate cell bounds
                    Vector3 cellMin = config.worldBounds.min + Vector3(
                        static_cast<float>(x) * config.cellSize.x,
                        static_cast<float>(y) * config.cellSize.y,
                        static_cast<float>(z) * config.cellSize.z
                    );
                    
                    Vector3 cellMax = Vector3(
                        std::min(cellMin.x + config.cellSize.x, config.worldBounds.max.x),
                        std::min(cellMin.y + config.cellSize.y, config.worldBounds.max.y),
                        std::min(cellMin.z + config.cellSize.z, config.worldBounds.max.z)
                    );

                    cells[cellID].id = cellID;
                    cells[cellID].bounds = AABB{cellMin, cellMax};
                    cells[cellID].center = (cellMin + cellMax) * 0.5f;

                    // Initialize visibility bitset with max objects capacity
                    visibilityData[cellID].Resize(config.maxObjects);
                }
            }
        }

        emptyBitSet.Resize(config.maxObjects);
    }

    void PVSData::Clear()
    {
        cells.clear();
        visibilityData.clear();
        gridDimensions = {0, 0, 0};
        invCellSize = Vector3(0.f, 0.f, 0.f);
        xyArea = 0;
    }

    PVSCellID PVSData::GetCellID(const Vector3 &position) const
    {
        // Fast path: check bounds and compute directly
        if (!IsInBounds(position)) {
            return INVALID_PVS_CELL;
        }
        return GetCellIDFast(position);
    }

    PVSCellCoord PVSData::GetCellCoord(const Vector3 &position) const
    {
        Vector3 offset = position - config.worldBounds.min;
        
        // Use multiplication with inverse instead of division
        // Use floor-like behavior for negative values: subtract 1 if negative with fractional part
        PVSCellCoord coord;
        float fx = offset.x * invCellSize.x;
        float fy = offset.y * invCellSize.y;
        float fz = offset.z * invCellSize.z;
        
        coord.x = static_cast<int32_t>(fx);
        coord.y = static_cast<int32_t>(fy);
        coord.z = static_cast<int32_t>(fz);
        
        // Correct for truncation toward zero vs floor (for negative values)
        if (fx < 0.0f && fx != static_cast<float>(coord.x)) coord.x -= 1;
        if (fy < 0.0f && fy != static_cast<float>(coord.y)) coord.y -= 1;
        if (fz < 0.0f && fz != static_cast<float>(coord.z)) coord.z -= 1;

        return coord;
    }

    PVSCellID PVSData::GetCellIDFromCoord(const PVSCellCoord &coord) const
    {
        // Check bounds
        if (coord.x < 0 || coord.x >= gridDimensions.x ||
            coord.y < 0 || coord.y >= gridDimensions.y ||
            coord.z < 0 || coord.z >= gridDimensions.z) {
            return INVALID_PVS_CELL;
        }

        // Use pre-computed xyArea
        return static_cast<PVSCellID>(
            coord.z * xyArea +
            coord.y * gridDimensions.x +
            coord.x
        );
    }

    PVSCellCoord PVSData::GetCoordFromCellID(PVSCellID cellID) const
    {
        if (cellID >= GetCellCount()) {
            return {-1, -1, -1};
        }

        // Use pre-computed xyArea
        PVSCellCoord coord;
        coord.z = static_cast<int32_t>(cellID) / xyArea;
        int32_t remainder = static_cast<int32_t>(cellID) % xyArea;
        coord.y = remainder / gridDimensions.x;
        coord.x = remainder % gridDimensions.x;

        return coord;
    }

    const PVSCell& PVSData::GetCell(PVSCellID cellID) const
    {
        static PVSCell invalidCell;
        if (!IsValidCell(cellID)) {
            return invalidCell;
        }
        return cells[cellID];
    }

    void PVSData::SetVisible(PVSCellID cellID, PVSObjectID objectID)
    {
        if (!IsValidCell(cellID) || objectID >= config.maxObjects) {
            return;
        }
        visibilityData[cellID].Set(objectID);
    }

    void PVSData::ClearVisible(PVSCellID cellID, PVSObjectID objectID)
    {
        if (!IsValidCell(cellID) || objectID >= config.maxObjects) {
            return;
        }
        visibilityData[cellID].Clear(objectID);
    }

    bool PVSData::IsVisible(PVSCellID cellID, PVSObjectID objectID) const
    {
        if (!IsValidCell(cellID) || objectID >= config.maxObjects) {
            return false;
        }
        return visibilityData[cellID].Test(objectID);
    }

    const PVSBitSet& PVSData::GetVisibilitySet(PVSCellID cellID) const
    {
        if (!IsValidCell(cellID)) {
            return emptyBitSet;
        }
        return visibilityData[cellID];
    }

    PVSBitSet& PVSData::GetMutableVisibilitySet(PVSCellID cellID)
    {
        static PVSBitSet dummyBitSet;
        if (!IsValidCell(cellID)) {
            return dummyBitSet;
        }
        return visibilityData[cellID];
    }

    void PVSData::SetAllVisible(PVSCellID cellID)
    {
        if (!IsValidCell(cellID)) {
            return;
        }
        visibilityData[cellID].SetAll();
    }

    void PVSData::ClearAllVisible(PVSCellID cellID)
    {
        if (!IsValidCell(cellID)) {
            return;
        }
        visibilityData[cellID].ClearAll();
    }

    void PVSData::LoadFromBakedData(const PVSBakedData &bakedData)
    {
        // Copy configuration
        config = bakedData.config;
        gridDimensions = bakedData.gridDimensions;

        // Pre-compute inverse cell sizes for fast lookup
        invCellSize.x = 1.0f / config.cellSize.x;
        invCellSize.y = 1.0f / config.cellSize.y;
        invCellSize.z = 1.0f / config.cellSize.z;
        
        // Pre-compute XY area for fast cell ID calculation
        xyArea = gridDimensions.x * gridDimensions.y;

        // Copy cells
        cells = bakedData.cells;

        // Copy visibility data
        visibilityData.resize(bakedData.visibilityData.size());
        for (size_t i = 0; i < bakedData.visibilityData.size(); ++i) {
            visibilityData[i].SetData(bakedData.visibilityData[i], bakedData.numObjects);
        }

        // Initialize empty bitset
        emptyBitSet.Resize(bakedData.numObjects);
    }

    void PVSData::ExportToBakedData(PVSBakedData &outBakedData) const
    {
        outBakedData.config = config;
        outBakedData.gridDimensions = gridDimensions;
        outBakedData.numObjects = config.maxObjects;
        outBakedData.cells = cells;

        // Export visibility data
        outBakedData.visibilityData.resize(visibilityData.size());
        for (size_t i = 0; i < visibilityData.size(); ++i) {
            outBakedData.visibilityData[i] = visibilityData[i].GetData();
        }
    }

} // namespace sky
