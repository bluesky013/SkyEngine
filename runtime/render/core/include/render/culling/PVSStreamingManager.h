//
// Created by SkyEngine on 2024/02/20.
//

#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <mutex>
#include <render/culling/PVSStreamingTypes.h>
#include <render/culling/PVSData.h>

namespace sky {

    class RenderPrimitive;
    class SceneView;

    /**
     * @brief Manages streaming of PVS data for large world scenarios
     * 
     * The PVSStreamingManager divides the world into sectors and streams
     * them in/out based on camera position. This allows handling worlds
     * much larger than available memory.
     * 
     * Key features:
     * - Automatic sector loading/unloading based on distance
     * - LRU cache for loaded sectors
     * - Async loading support
     * - Cross-sector visibility queries
     */
    class PVSStreamingManager {
    public:
        PVSStreamingManager() = default;
        ~PVSStreamingManager() = default;

        /**
         * @brief Initialize the streaming manager
         * @param config Streaming configuration
         */
        void Initialize(const PVSStreamingConfig &config);

        /**
         * @brief Clear all data and reset
         */
        void Clear();

        /**
         * @brief Set the data provider for loading sector data
         * 
         * The provider is called when a sector needs to be loaded.
         * It should return true and fill outData if the sector exists.
         * 
         * @param provider Function to provide sector data
         */
        void SetDataProvider(PVSSectorDataProvider provider);

        /**
         * @brief Update streaming based on viewer position
         * 
         * Call this every frame with the camera position. It will:
         * - Load nearby sectors within loadRadius
         * - Unload distant sectors beyond unloadRadius
         * - Respect maxLoadedSectors limit using LRU
         * 
         * @param viewerPosition Current camera/viewer position
         * @param frameNumber Current frame number for LRU tracking
         */
        void Update(const Vector3 &viewerPosition, uint64_t frameNumber);

        /**
         * @brief Request a specific sector to be loaded
         * @param coord Sector coordinates
         * @param callback Optional callback when load completes
         */
        void RequestSectorLoad(const PVSSectorCoord &coord, PVSSectorLoadCallback callback = nullptr);

        /**
         * @brief Request a sector to be unloaded
         * @param coord Sector coordinates
         */
        void RequestSectorUnload(const PVSSectorCoord &coord);

        /**
         * @brief Check if a sector is loaded
         * @param coord Sector coordinates
         * @return True if sector is loaded and ready
         */
        bool IsSectorLoaded(const PVSSectorCoord &coord) const;

        /**
         * @brief Get the sector coordinate for a world position
         * @param position World position
         * @return Sector coordinates
         */
        PVSSectorCoord GetSectorCoord(const Vector3 &position) const;

        /**
         * @brief Get the sector ID for coordinates
         * @param coord Sector coordinates
         * @return Sector ID or INVALID_PVS_SECTOR if not loaded
         */
        PVSSectorID GetSectorID(const PVSSectorCoord &coord) const;

        /**
         * @brief Query visibility across loaded sectors
         * 
         * This queries visibility from the viewer position across all
         * relevant loaded sectors.
         * 
         * @param viewPosition Camera/view position
         * @param sceneView Optional scene view for frustum culling
         * @param result Vector to store visible primitives
         * @return True if query was successful (relevant sectors loaded)
         */
        bool QueryVisiblePrimitives(
            const Vector3 &viewPosition,
            const SceneView *sceneView,
            std::vector<RenderPrimitive*> &result) const;

        /**
         * @brief Iterate over visible primitives without allocation
         * 
         * @param viewPosition Camera/view position
         * @param sceneView Optional scene view for frustum culling
         * @param callback Function called for each visible primitive
         * @return Number of visible primitives, or 0 if sectors not loaded
         */
        template <typename Func>
        uint32_t ForEachVisiblePrimitive(
            const Vector3 &viewPosition,
            const SceneView *sceneView,
            Func &&callback) const
        {
            PVSSectorCoord sectorCoord = GetSectorCoord(viewPosition);
            
            auto it = loadedSectors.find(sectorCoord);
            if (it == loadedSectors.end() || it->second.info.state != PVSSectorState::LOADED) {
                return 0;
            }

            const LoadedSector &sector = it->second;
            
            // Get cell ID within the sector
            PVSCellID cellID = sector.pvsData.GetCellID(viewPosition);
            if (cellID == INVALID_PVS_CELL) {
                return 0;
            }

            uint32_t count = 0;
            const PVSBitSet &visibility = sector.pvsData.GetVisibilitySet(cellID);
            
            visibility.ForEachSetBit([&](uint32_t localObjID) {
                if (localObjID < sector.primitives.size()) {
                    auto *primitive = sector.primitives[localObjID];
                    if (primitive != nullptr) {
                        callback(primitive);
                        ++count;
                    }
                }
            });
            
            return count;
        }

        /**
         * @brief Check if a position has valid visibility data loaded
         * @param position World position
         * @return True if the sector containing position is loaded
         */
        bool IsPositionLoaded(const Vector3 &position) const;

        /**
         * @brief Get the number of currently loaded sectors
         */
        uint32_t GetLoadedSectorCount() const { return static_cast<uint32_t>(loadedSectors.size()); }

        /**
         * @brief Get all loaded sector coordinates
         * @param result Vector to store coordinates
         */
        void GetLoadedSectors(std::vector<PVSSectorCoord> &result) const;

        /**
         * @brief Get streaming statistics
         */
        struct Statistics {
            uint32_t loadedSectors = 0;
            uint32_t pendingLoads = 0;
            uint32_t pendingUnloads = 0;
            size_t totalMemoryUsed = 0;
            float averageSectorSize = 0.0f;
        };
        Statistics GetStatistics() const;

        /**
         * @brief Register a primitive with a specific sector
         * 
         * Objects that span multiple sectors should be registered with each sector.
         * 
         * @param coord Sector coordinates
         * @param primitive Render primitive
         * @param localObjectID The object ID within the sector's PVS data
         */
        void RegisterPrimitive(const PVSSectorCoord &coord, RenderPrimitive *primitive, uint32_t localObjectID);

        /**
         * @brief Unregister a primitive from a sector
         * @param coord Sector coordinates
         * @param primitive Render primitive
         */
        void UnregisterPrimitive(const PVSSectorCoord &coord, RenderPrimitive *primitive);

        /**
         * @brief Get the configuration
         */
        const PVSStreamingConfig& GetConfig() const { return config; }

    private:
        /**
         * @brief Internal representation of a loaded sector
         */
        struct LoadedSector {
            PVSSectorInfo info;
            PVSData pvsData;
            std::vector<RenderPrimitive*> primitives;  // Local ID -> primitive
            size_t memorySize = 0;
            
            // For tracking primitive to local ID mapping
            std::unordered_map<RenderPrimitive*, uint32_t> primitiveToLocalID;
        };

        /**
         * @brief Load a sector synchronously
         */
        bool LoadSectorInternal(const PVSSectorCoord &coord);

        /**
         * @brief Unload a sector
         */
        void UnloadSectorInternal(const PVSSectorCoord &coord);

        /**
         * @brief Get sectors to load based on distance
         */
        void GetSectorsToLoad(const Vector3 &position, std::vector<PVSSectorCoord> &result) const;

        /**
         * @brief Get sectors to unload based on distance
         */
        void GetSectorsToUnload(const Vector3 &position, std::vector<PVSSectorCoord> &result) const;

        /**
         * @brief Enforce LRU limit on loaded sectors
         */
        void EnforceSectorLimit();

        /**
         * @brief Calculate distance from sector center to position
         */
        float GetDistanceToSector(const PVSSectorCoord &coord, const Vector3 &position) const;

        /**
         * @brief Get sector bounds from coordinates
         */
        AABB GetSectorBounds(const PVSSectorCoord &coord) const;

        /**
         * @brief Get sector center from coordinates
         */
        Vector3 GetSectorCenter(const PVSSectorCoord &coord) const;

        PVSStreamingConfig config;
        bool initialized = false;

        // Sector grid info
        PVSSectorCoord gridDimensions;
        Vector3 invSectorSize;  // 1/sectorSize for fast lookup

        // Loaded sectors
        std::unordered_map<PVSSectorCoord, LoadedSector, PVSSectorCoordHash> loadedSectors;

        // Pending operations
        std::unordered_set<PVSSectorCoord, PVSSectorCoordHash> pendingLoads;
        std::unordered_set<PVSSectorCoord, PVSSectorCoordHash> pendingUnloads;

        // Data provider
        PVSSectorDataProvider dataProvider;

        // Load callbacks
        std::unordered_map<PVSSectorCoord, std::vector<PVSSectorLoadCallback>, PVSSectorCoordHash> loadCallbacks;

        // Current viewer position for LRU
        Vector3 currentViewerPosition;
        uint64_t currentFrame = 0;
    };

} // namespace sky
