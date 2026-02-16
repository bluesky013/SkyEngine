//
// Created by SkyEngine on 2024/02/16.
//

#include <render/culling/PVSBakedData.h>
#include <framework/serialization/BinaryArchive.h>

namespace sky {

    void PVSBakedData::Save(BinaryOutputArchive &archive) const
    {
        // Save version
        archive.SaveValue(VERSION);

        // Save config
        archive.SaveValue(config.worldBounds.min.x);
        archive.SaveValue(config.worldBounds.min.y);
        archive.SaveValue(config.worldBounds.min.z);
        archive.SaveValue(config.worldBounds.max.x);
        archive.SaveValue(config.worldBounds.max.y);
        archive.SaveValue(config.worldBounds.max.z);
        archive.SaveValue(config.cellSize.x);
        archive.SaveValue(config.cellSize.y);
        archive.SaveValue(config.cellSize.z);
        archive.SaveValue(config.maxObjects);
        archive.SaveValue(config.enablePortals);

        // Save grid dimensions
        archive.SaveValue(gridDimensions.x);
        archive.SaveValue(gridDimensions.y);
        archive.SaveValue(gridDimensions.z);

        // Save number of objects
        archive.SaveValue(numObjects);

        // Save cells
        uint32_t numCells = static_cast<uint32_t>(cells.size());
        archive.SaveValue(numCells);
        for (const auto &cell : cells) {
            archive.SaveValue(cell.id);
            archive.SaveValue(cell.bounds.min.x);
            archive.SaveValue(cell.bounds.min.y);
            archive.SaveValue(cell.bounds.min.z);
            archive.SaveValue(cell.bounds.max.x);
            archive.SaveValue(cell.bounds.max.y);
            archive.SaveValue(cell.bounds.max.z);
            archive.SaveValue(cell.center.x);
            archive.SaveValue(cell.center.y);
            archive.SaveValue(cell.center.z);
        }

        // Save visibility data
        uint32_t numVisibilitySets = static_cast<uint32_t>(visibilityData.size());
        archive.SaveValue(numVisibilitySets);
        for (const auto &bitsetData : visibilityData) {
            uint32_t numWords = static_cast<uint32_t>(bitsetData.size());
            archive.SaveValue(numWords);
            if (numWords > 0) {
                archive.SaveValue(
                    reinterpret_cast<const char*>(bitsetData.data()),
                    numWords * sizeof(uint64_t)
                );
            }
        }

        // Save object names
        uint32_t numNames = static_cast<uint32_t>(objectNames.size());
        archive.SaveValue(numNames);
        for (const auto &name : objectNames) {
            archive.SaveValue(name);
        }
    }

    void PVSBakedData::Load(BinaryInputArchive &archive)
    {
        // Load and verify version
        uint32_t version = 0;
        archive.LoadValue(version);
        if (version != VERSION) {
            // Handle version mismatch - for now, we only support current version
            return;
        }

        // Load config
        archive.LoadValue(config.worldBounds.min.x);
        archive.LoadValue(config.worldBounds.min.y);
        archive.LoadValue(config.worldBounds.min.z);
        archive.LoadValue(config.worldBounds.max.x);
        archive.LoadValue(config.worldBounds.max.y);
        archive.LoadValue(config.worldBounds.max.z);
        archive.LoadValue(config.cellSize.x);
        archive.LoadValue(config.cellSize.y);
        archive.LoadValue(config.cellSize.z);
        archive.LoadValue(config.maxObjects);
        archive.LoadValue(config.enablePortals);

        // Load grid dimensions
        archive.LoadValue(gridDimensions.x);
        archive.LoadValue(gridDimensions.y);
        archive.LoadValue(gridDimensions.z);

        // Load number of objects
        archive.LoadValue(numObjects);

        // Load cells
        uint32_t numCells = 0;
        archive.LoadValue(numCells);
        cells.resize(numCells);
        for (auto &cell : cells) {
            archive.LoadValue(cell.id);
            archive.LoadValue(cell.bounds.min.x);
            archive.LoadValue(cell.bounds.min.y);
            archive.LoadValue(cell.bounds.min.z);
            archive.LoadValue(cell.bounds.max.x);
            archive.LoadValue(cell.bounds.max.y);
            archive.LoadValue(cell.bounds.max.z);
            archive.LoadValue(cell.center.x);
            archive.LoadValue(cell.center.y);
            archive.LoadValue(cell.center.z);
        }

        // Load visibility data
        uint32_t numVisibilitySets = 0;
        archive.LoadValue(numVisibilitySets);
        visibilityData.resize(numVisibilitySets);
        for (auto &bitsetData : visibilityData) {
            uint32_t numWords = 0;
            archive.LoadValue(numWords);
            bitsetData.resize(numWords);
            if (numWords > 0) {
                archive.LoadValue(
                    reinterpret_cast<char*>(bitsetData.data()),
                    numWords * sizeof(uint64_t)
                );
            }
        }

        // Load object names
        uint32_t numNames = 0;
        archive.LoadValue(numNames);
        objectNames.resize(numNames);
        for (auto &name : objectNames) {
            archive.LoadValue(name);
        }
    }

    size_t PVSBakedData::GetMemorySize() const
    {
        size_t size = sizeof(PVSBakedData);
        size += cells.size() * sizeof(PVSCell);
        for (const auto &bitsetData : visibilityData) {
            size += bitsetData.size() * sizeof(uint64_t);
        }
        for (const auto &name : objectNames) {
            size += name.size();
        }
        return size;
    }

    PVSBakedData::Statistics PVSBakedData::GetStatistics() const
    {
        Statistics stats;
        stats.totalCells = static_cast<uint32_t>(cells.size());
        stats.totalObjects = numObjects;
        
        // Count visible pairs
        uint32_t totalVisible = 0;
        for (const auto &bitsetData : visibilityData) {
            for (uint64_t word : bitsetData) {
#ifdef _MSC_VER
                totalVisible += static_cast<uint32_t>(__popcnt64(word));
#else
                totalVisible += static_cast<uint32_t>(__builtin_popcountll(word));
#endif
            }
        }
        stats.totalVisiblePairs = totalVisible;
        
        if (stats.totalCells > 0) {
            stats.averageVisibleObjects = 
                static_cast<float>(totalVisible) / static_cast<float>(stats.totalCells);
        }
        
        // Calculate raw vs compressed size ratio
        size_t rawSize = stats.totalCells * stats.totalObjects;  // One bit per pair
        stats.rawDataSize = GetMemorySize();
        
        if (rawSize > 0) {
            stats.compressionRatio = 
                static_cast<float>(stats.rawDataSize) / static_cast<float>(rawSize / 8);
        }
        
        return stats;
    }

} // namespace sky
