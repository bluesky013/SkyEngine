//
// Created by SkyEngine on 2024/02/16.
//

#pragma once

#include <functional>
#include <vector>
#include <string>
#include <atomic>
#include <render/culling/PVSTypes.h>
#include <render/culling/PVSBakedData.h>
#include <render/culling/PVSCulling.h>
#include <core/shapes/AABB.h>

namespace sky {

    /**
     * @brief Progress callback for PVS baking
     * @param progress Progress value from 0.0 to 1.0
     * @param message Status message
     */
    using PVSBakeProgressCallback = std::function<void(float progress, const std::string &message)>;

    /**
     * @brief Visibility test function type
     * @param cellBounds Bounds of the cell
     * @param cellCenter Center of the cell
     * @param objectBounds Bounds of the object
     * @param objectIndex Index of the object
     * @return true if the object should be considered visible from the cell
     */
    using PVSVisibilityTestFunc = std::function<bool(
        const AABB &cellBounds,
        const Vector3 &cellCenter,
        const AABB &objectBounds,
        uint32_t objectIndex
    )>;

    /**
     * @brief Configuration for PVS baking
     */
    struct PVSBakeConfig {
        // PVS grid configuration
        PVSConfig pvsConfig;
        
        // Visibility computation method
        enum class Method {
            DISTANCE,       // Simple distance-based culling
            RAYCAST,        // Ray-casting based visibility
            CUSTOM          // Custom visibility function
        };
        Method method = Method::DISTANCE;
        
        // Distance-based parameters
        float maxVisibilityDistance = 100.0f;
        
        // Ray-cast parameters
        uint32_t raysPerCell = 64;     // Number of rays to cast per cell
        uint32_t rayBounces = 1;       // Number of bounces for indirect visibility
        
        // Custom visibility function
        PVSVisibilityTestFunc customTestFunc;
        
        // Parallelization
        uint32_t numThreads = 0;  // 0 = auto-detect
        
        // Progress callback
        PVSBakeProgressCallback progressCallback;
    };

    /**
     * @brief Result of PVS baking operation
     */
    struct PVSBakeResult {
        bool success = false;
        std::string errorMessage;
        
        // Timing
        float totalTimeSeconds = 0.0f;
        float computeTimeSeconds = 0.0f;
        
        // Statistics
        PVSBakedData::Statistics statistics;
    };

    /**
     * @brief Object to be included in PVS baking
     */
    struct PVSBakeObject {
        AABB bounds;
        std::string name;
        uint32_t flags = 0;  // User-defined flags (e.g., static, dynamic, occluder)
    };

    /**
     * @brief PVS Baker - computes and bakes visibility data
     * 
     * The PVSBaker takes a scene description (objects with bounds) and
     * computes visibility data that can be serialized and loaded at runtime.
     */
    class PVSBaker {
    public:
        PVSBaker() = default;
        ~PVSBaker() = default;

        /**
         * @brief Add an object to be baked
         * @param object Object description
         * @return Object index for reference
         */
        uint32_t AddObject(const PVSBakeObject &object);

        /**
         * @brief Add multiple objects
         * @param objects Vector of objects
         */
        void AddObjects(const std::vector<PVSBakeObject> &objects);

        /**
         * @brief Clear all objects
         */
        void ClearObjects();

        /**
         * @brief Get the number of objects
         */
        uint32_t GetObjectCount() const { return static_cast<uint32_t>(objects.size()); }

        /**
         * @brief Get an object by index
         */
        const PVSBakeObject& GetObject(uint32_t index) const { return objects[index]; }

        /**
         * @brief Bake the PVS data
         * @param config Baking configuration
         * @param outData Output baked data
         * @return Baking result
         */
        PVSBakeResult Bake(const PVSBakeConfig &config, PVSBakedData &outData);

        /**
         * @brief Cancel ongoing baking operation
         */
        void Cancel();

        /**
         * @brief Check if baking is in progress
         */
        bool IsBaking() const { return baking.load(); }

    private:
        /**
         * @brief Compute visibility using distance method
         */
        void BakeDistanceBased(
            const PVSBakeConfig &config,
            PVSBakedData &outData,
            PVSBakeResult &result);

        /**
         * @brief Compute visibility using ray-cast method
         */
        void BakeRayCast(
            const PVSBakeConfig &config,
            PVSBakedData &outData,
            PVSBakeResult &result);

        /**
         * @brief Compute visibility using custom function
         */
        void BakeCustom(
            const PVSBakeConfig &config,
            PVSBakedData &outData,
            PVSBakeResult &result);

        /**
         * @brief Initialize output data structure
         */
        void InitializeOutputData(const PVSBakeConfig &config, PVSBakedData &outData);

        /**
         * @brief Report progress
         */
        void ReportProgress(
            const PVSBakeConfig &config,
            float progress,
            const std::string &message);

        std::vector<PVSBakeObject> objects;
        std::atomic<bool> baking{false};
        std::atomic<bool> cancelRequested{false};
    };

} // namespace sky
