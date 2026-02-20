//
// Created by SkyEngine on 2024/02/20.
//

#include <render/culling/PVSStreamingManager.h>
#include <render/SceneView.h>
#include <algorithm>
#include <cmath>

namespace sky {

    void PVSStreamingManager::Initialize(const PVSStreamingConfig &cfg)
    {
        config = cfg;
        
        // Calculate grid dimensions
        Vector3 worldSize = config.worldBounds.max - config.worldBounds.min;
        
        gridDimensions.x = static_cast<int32_t>(std::ceil(worldSize.x / config.sectorSize.x));
        gridDimensions.y = static_cast<int32_t>(std::ceil(worldSize.y / config.sectorSize.y));
        gridDimensions.z = static_cast<int32_t>(std::ceil(worldSize.z / config.sectorSize.z));
        
        // Ensure at least one sector
        gridDimensions.x = std::max(1, gridDimensions.x);
        gridDimensions.y = std::max(1, gridDimensions.y);
        gridDimensions.z = std::max(1, gridDimensions.z);
        
        // Pre-compute inverse sizes
        invSectorSize.x = 1.0f / config.sectorSize.x;
        invSectorSize.y = 1.0f / config.sectorSize.y;
        invSectorSize.z = 1.0f / config.sectorSize.z;
        
        initialized = true;
    }

    void PVSStreamingManager::Clear()
    {
        loadedSectors.clear();
        pendingLoads.clear();
        pendingUnloads.clear();
        loadCallbacks.clear();
        initialized = false;
    }

    void PVSStreamingManager::SetDataProvider(PVSSectorDataProvider provider)
    {
        dataProvider = std::move(provider);
    }

    PVSSectorCoord PVSStreamingManager::GetSectorCoord(const Vector3 &position) const
    {
        Vector3 offset = position - config.worldBounds.min;
        
        PVSSectorCoord coord;
        coord.x = static_cast<int32_t>(offset.x * invSectorSize.x);
        coord.y = static_cast<int32_t>(offset.y * invSectorSize.y);
        coord.z = static_cast<int32_t>(offset.z * invSectorSize.z);
        
        return coord;
    }

    PVSSectorID PVSStreamingManager::GetSectorID(const PVSSectorCoord &coord) const
    {
        auto it = loadedSectors.find(coord);
        if (it != loadedSectors.end()) {
            return it->second.info.id;
        }
        return INVALID_PVS_SECTOR;
    }

    AABB PVSStreamingManager::GetSectorBounds(const PVSSectorCoord &coord) const
    {
        Vector3 minPt = config.worldBounds.min + Vector3(
            static_cast<float>(coord.x) * config.sectorSize.x,
            static_cast<float>(coord.y) * config.sectorSize.y,
            static_cast<float>(coord.z) * config.sectorSize.z
        );
        
        Vector3 maxPt = Vector3(
            std::min(minPt.x + config.sectorSize.x, config.worldBounds.max.x),
            std::min(minPt.y + config.sectorSize.y, config.worldBounds.max.y),
            std::min(minPt.z + config.sectorSize.z, config.worldBounds.max.z)
        );
        
        return AABB{minPt, maxPt};
    }

    Vector3 PVSStreamingManager::GetSectorCenter(const PVSSectorCoord &coord) const
    {
        AABB bounds = GetSectorBounds(coord);
        return (bounds.min + bounds.max) * 0.5f;
    }

    float PVSStreamingManager::GetDistanceToSector(const PVSSectorCoord &coord, const Vector3 &position) const
    {
        Vector3 center = GetSectorCenter(coord);
        Vector3 diff = position - center;
        return std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);
    }

    bool PVSStreamingManager::IsSectorLoaded(const PVSSectorCoord &coord) const
    {
        auto it = loadedSectors.find(coord);
        return it != loadedSectors.end() && it->second.info.state == PVSSectorState::LOADED;
    }

    bool PVSStreamingManager::IsPositionLoaded(const Vector3 &position) const
    {
        return IsSectorLoaded(GetSectorCoord(position));
    }

    void PVSStreamingManager::Update(const Vector3 &viewerPosition, uint64_t frameNumber)
    {
        if (!initialized) {
            return;
        }

        currentViewerPosition = viewerPosition;
        currentFrame = frameNumber;

        // Get sectors to load
        std::vector<PVSSectorCoord> toLoad;
        GetSectorsToLoad(viewerPosition, toLoad);
        
        // Get sectors to unload
        std::vector<PVSSectorCoord> toUnload;
        GetSectorsToUnload(viewerPosition, toUnload);

        // Process unloads first to free memory
        for (const auto &coord : toUnload) {
            UnloadSectorInternal(coord);
        }

        // Process loads
        for (const auto &coord : toLoad) {
            if (!IsSectorLoaded(coord) && pendingLoads.find(coord) == pendingLoads.end()) {
                LoadSectorInternal(coord);
            }
        }

        // Enforce memory limit
        EnforceSectorLimit();

        // Update LRU for current sector
        PVSSectorCoord currentCoord = GetSectorCoord(viewerPosition);
        auto it = loadedSectors.find(currentCoord);
        if (it != loadedSectors.end()) {
            it->second.info.lastUsedFrame = frameNumber;
            it->second.info.distanceToViewer = 0.0f;
        }
    }

    void PVSStreamingManager::GetSectorsToLoad(const Vector3 &position, std::vector<PVSSectorCoord> &result) const
    {
        result.clear();
        
        PVSSectorCoord centerCoord = GetSectorCoord(position);
        
        // Calculate how many sectors fit in load radius
        int32_t rangeX = static_cast<int32_t>(std::ceil(config.loadRadius / config.sectorSize.x));
        int32_t rangeY = static_cast<int32_t>(std::ceil(config.loadRadius / config.sectorSize.y));
        int32_t rangeZ = static_cast<int32_t>(std::ceil(config.loadRadius / config.sectorSize.z));
        
        for (int32_t dz = -rangeZ; dz <= rangeZ; ++dz) {
            for (int32_t dy = -rangeY; dy <= rangeY; ++dy) {
                for (int32_t dx = -rangeX; dx <= rangeX; ++dx) {
                    PVSSectorCoord coord = {
                        centerCoord.x + dx,
                        centerCoord.y + dy,
                        centerCoord.z + dz
                    };
                    
                    // Check if within grid bounds
                    if (coord.x < 0 || coord.x >= gridDimensions.x ||
                        coord.y < 0 || coord.y >= gridDimensions.y ||
                        coord.z < 0 || coord.z >= gridDimensions.z) {
                        continue;
                    }
                    
                    // Check distance
                    float dist = GetDistanceToSector(coord, position);
                    if (dist <= config.loadRadius) {
                        result.push_back(coord);
                    }
                }
            }
        }
        
        // Sort by distance (closest first)
        std::sort(result.begin(), result.end(), 
            [this, &position](const PVSSectorCoord &a, const PVSSectorCoord &b) {
                return GetDistanceToSector(a, position) < GetDistanceToSector(b, position);
            });
    }

    void PVSStreamingManager::GetSectorsToUnload(const Vector3 &position, std::vector<PVSSectorCoord> &result) const
    {
        result.clear();
        
        for (const auto &pair : loadedSectors) {
            float dist = GetDistanceToSector(pair.first, position);
            if (dist > config.unloadRadius) {
                result.push_back(pair.first);
            }
        }
    }

    bool PVSStreamingManager::LoadSectorInternal(const PVSSectorCoord &coord)
    {
        if (!dataProvider) {
            return false;
        }
        
        // Get sector data from provider
        PVSSectorBakedData bakedData;
        if (!dataProvider(coord, bakedData)) {
            return false;
        }
        
        // Create loaded sector
        LoadedSector sector;
        sector.info.id = static_cast<PVSSectorID>(
            coord.z * (gridDimensions.x * gridDimensions.y) +
            coord.y * gridDimensions.x + coord.x);
        sector.info.coord = coord;
        sector.info.bounds = GetSectorBounds(coord);
        sector.info.center = GetSectorCenter(coord);
        sector.info.state = PVSSectorState::LOADED;
        sector.info.lastUsedFrame = currentFrame;
        sector.info.distanceToViewer = GetDistanceToSector(coord, currentViewerPosition);
        
        // Load PVS data
        sector.pvsData.LoadFromBakedData(bakedData.pvsData);
        
        // Reserve primitive slots
        sector.primitives.resize(bakedData.pvsData.numObjects, nullptr);
        
        // Calculate memory size
        sector.memorySize = bakedData.GetMemorySize();
        
        // Store loaded sector
        loadedSectors[coord] = std::move(sector);
        
        // Call load callbacks
        auto callbackIt = loadCallbacks.find(coord);
        if (callbackIt != loadCallbacks.end()) {
            for (auto &callback : callbackIt->second) {
                if (callback) {
                    callback(loadedSectors[coord].info.id, true);
                }
            }
            loadCallbacks.erase(callbackIt);
        }
        
        return true;
    }

    void PVSStreamingManager::UnloadSectorInternal(const PVSSectorCoord &coord)
    {
        auto it = loadedSectors.find(coord);
        if (it == loadedSectors.end()) {
            return;
        }
        
        // Mark as unloading
        it->second.info.state = PVSSectorState::UNLOADING;
        
        // Clear primitives
        it->second.primitives.clear();
        it->second.primitiveToLocalID.clear();
        
        // Remove from map
        loadedSectors.erase(it);
    }

    void PVSStreamingManager::RequestSectorLoad(const PVSSectorCoord &coord, PVSSectorLoadCallback callback)
    {
        if (callback) {
            loadCallbacks[coord].push_back(callback);
        }
        
        if (IsSectorLoaded(coord)) {
            // Already loaded, call callback immediately
            if (callback) {
                callback(GetSectorID(coord), true);
            }
            loadCallbacks.erase(coord);
            return;
        }
        
        pendingLoads.insert(coord);
        LoadSectorInternal(coord);
        pendingLoads.erase(coord);
    }

    void PVSStreamingManager::RequestSectorUnload(const PVSSectorCoord &coord)
    {
        pendingUnloads.insert(coord);
        UnloadSectorInternal(coord);
        pendingUnloads.erase(coord);
    }

    void PVSStreamingManager::EnforceSectorLimit()
    {
        if (loadedSectors.size() <= config.maxLoadedSectors) {
            return;
        }
        
        // Build list of sectors sorted by LRU (oldest first)
        std::vector<std::pair<PVSSectorCoord, uint64_t>> sectors;
        for (const auto &pair : loadedSectors) {
            sectors.emplace_back(pair.first, pair.second.info.lastUsedFrame);
        }
        
        // Sort by last used frame (oldest first)
        std::sort(sectors.begin(), sectors.end(),
            [](const auto &a, const auto &b) {
                return a.second < b.second;
            });
        
        // Unload oldest sectors until under limit
        size_t toRemove = loadedSectors.size() - config.maxLoadedSectors;
        for (size_t i = 0; i < toRemove && i < sectors.size(); ++i) {
            UnloadSectorInternal(sectors[i].first);
        }
    }

    bool PVSStreamingManager::QueryVisiblePrimitives(
        const Vector3 &viewPosition,
        const SceneView *sceneView,
        std::vector<RenderPrimitive*> &result) const
    {
        result.clear();
        
        PVSSectorCoord sectorCoord = GetSectorCoord(viewPosition);
        
        auto it = loadedSectors.find(sectorCoord);
        if (it == loadedSectors.end() || it->second.info.state != PVSSectorState::LOADED) {
            return false;
        }
        
        const LoadedSector &sector = it->second;
        
        // Get cell ID within the sector
        PVSCellID cellID = sector.pvsData.GetCellID(viewPosition);
        if (cellID == INVALID_PVS_CELL) {
            return false;
        }
        
        const PVSBitSet &visibility = sector.pvsData.GetVisibilitySet(cellID);
        
        // Pre-reserve based on visible count
        result.reserve(visibility.CountSet());
        
        visibility.ForEachSetBit([&](uint32_t localObjID) {
            if (localObjID < sector.primitives.size()) {
                RenderPrimitive *primitive = sector.primitives[localObjID];
                if (primitive != nullptr) {
                    if (sceneView == nullptr || sceneView->FrustumCulling(primitive->worldBound)) {
                        result.push_back(primitive);
                    }
                }
            }
        });
        
        return true;
    }

    void PVSStreamingManager::GetLoadedSectors(std::vector<PVSSectorCoord> &result) const
    {
        result.clear();
        result.reserve(loadedSectors.size());
        for (const auto &pair : loadedSectors) {
            result.push_back(pair.first);
        }
    }

    PVSStreamingManager::Statistics PVSStreamingManager::GetStatistics() const
    {
        Statistics stats;
        stats.loadedSectors = static_cast<uint32_t>(loadedSectors.size());
        stats.pendingLoads = static_cast<uint32_t>(pendingLoads.size());
        stats.pendingUnloads = static_cast<uint32_t>(pendingUnloads.size());
        
        for (const auto &pair : loadedSectors) {
            stats.totalMemoryUsed += pair.second.memorySize;
        }
        
        if (stats.loadedSectors > 0) {
            stats.averageSectorSize = static_cast<float>(stats.totalMemoryUsed) / 
                                      static_cast<float>(stats.loadedSectors);
        }
        
        return stats;
    }

    void PVSStreamingManager::RegisterPrimitive(
        const PVSSectorCoord &coord,
        RenderPrimitive *primitive,
        uint32_t localObjectID)
    {
        auto it = loadedSectors.find(coord);
        if (it == loadedSectors.end()) {
            return;
        }
        
        LoadedSector &sector = it->second;
        
        if (localObjectID >= sector.primitives.size()) {
            sector.primitives.resize(localObjectID + 1, nullptr);
        }
        
        sector.primitives[localObjectID] = primitive;
        sector.primitiveToLocalID[primitive] = localObjectID;
    }

    void PVSStreamingManager::UnregisterPrimitive(
        const PVSSectorCoord &coord,
        RenderPrimitive *primitive)
    {
        auto it = loadedSectors.find(coord);
        if (it == loadedSectors.end()) {
            return;
        }
        
        LoadedSector &sector = it->second;
        
        auto idIt = sector.primitiveToLocalID.find(primitive);
        if (idIt != sector.primitiveToLocalID.end()) {
            uint32_t localID = idIt->second;
            if (localID < sector.primitives.size()) {
                sector.primitives[localID] = nullptr;
            }
            sector.primitiveToLocalID.erase(idIt);
        }
    }

} // namespace sky
