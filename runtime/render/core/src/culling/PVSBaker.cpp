//
// Created by SkyEngine on 2024/02/16.
//

#include <render/culling/PVSBaker.h>
#include <render/culling/SIMDUtils.h>
#include <chrono>
#include <cmath>
#include <algorithm>

namespace sky {

    uint32_t PVSBaker::AddObject(const PVSBakeObject &object)
    {
        uint32_t index = static_cast<uint32_t>(objects.size());
        objects.push_back(object);
        return index;
    }

    void PVSBaker::AddObjects(const std::vector<PVSBakeObject> &newObjects)
    {
        objects.reserve(objects.size() + newObjects.size());
        for (const auto &obj : newObjects) {
            objects.push_back(obj);
        }
    }

    void PVSBaker::ClearObjects()
    {
        objects.clear();
    }

    void PVSBaker::Cancel()
    {
        cancelRequested.store(true);
    }

    void PVSBaker::InitializeOutputData(const PVSBakeConfig &config, PVSBakedData &outData)
    {
        outData.config = config.pvsConfig;
        outData.numObjects = static_cast<uint32_t>(objects.size());

        // Calculate grid dimensions
        Vector3 worldSize = config.pvsConfig.worldBounds.max - config.pvsConfig.worldBounds.min;
        
        outData.gridDimensions.x = static_cast<int32_t>(
            std::ceil(worldSize.x / config.pvsConfig.cellSize.x));
        outData.gridDimensions.y = static_cast<int32_t>(
            std::ceil(worldSize.y / config.pvsConfig.cellSize.y));
        outData.gridDimensions.z = static_cast<int32_t>(
            std::ceil(worldSize.z / config.pvsConfig.cellSize.z));

        // Ensure at least one cell
        outData.gridDimensions.x = std::max(1, outData.gridDimensions.x);
        outData.gridDimensions.y = std::max(1, outData.gridDimensions.y);
        outData.gridDimensions.z = std::max(1, outData.gridDimensions.z);

        uint32_t totalCells = static_cast<uint32_t>(
            outData.gridDimensions.x * outData.gridDimensions.y * outData.gridDimensions.z);

        // Initialize cells
        outData.cells.resize(totalCells);
        for (int32_t z = 0; z < outData.gridDimensions.z; ++z) {
            for (int32_t y = 0; y < outData.gridDimensions.y; ++y) {
                for (int32_t x = 0; x < outData.gridDimensions.x; ++x) {
                    uint32_t cellID = static_cast<uint32_t>(
                        z * (outData.gridDimensions.x * outData.gridDimensions.y) +
                        y * outData.gridDimensions.x + x);

                    Vector3 cellMin = config.pvsConfig.worldBounds.min + Vector3(
                        static_cast<float>(x) * config.pvsConfig.cellSize.x,
                        static_cast<float>(y) * config.pvsConfig.cellSize.y,
                        static_cast<float>(z) * config.pvsConfig.cellSize.z
                    );

                    Vector3 cellMax = Vector3(
                        std::min(cellMin.x + config.pvsConfig.cellSize.x, 
                                 config.pvsConfig.worldBounds.max.x),
                        std::min(cellMin.y + config.pvsConfig.cellSize.y, 
                                 config.pvsConfig.worldBounds.max.y),
                        std::min(cellMin.z + config.pvsConfig.cellSize.z, 
                                 config.pvsConfig.worldBounds.max.z)
                    );

                    outData.cells[cellID].id = cellID;
                    outData.cells[cellID].bounds = AABB{cellMin, cellMax};
                    outData.cells[cellID].center = (cellMin + cellMax) * 0.5f;
                }
            }
        }

        // Initialize visibility data (one bitset per cell)
        uint32_t wordsPerCell = (outData.numObjects + 63) / 64;
        outData.visibilityData.resize(totalCells);
        for (auto &bitset : outData.visibilityData) {
            bitset.resize(wordsPerCell, 0);
        }

        // Copy object names
        outData.objectNames.resize(objects.size());
        for (size_t i = 0; i < objects.size(); ++i) {
            outData.objectNames[i] = objects[i].name;
        }
    }

    void PVSBaker::ReportProgress(
        const PVSBakeConfig &config,
        float progress,
        const std::string &message)
    {
        if (config.progressCallback) {
            config.progressCallback(progress, message);
        }
    }

    PVSBakeResult PVSBaker::Bake(const PVSBakeConfig &config, PVSBakedData &outData)
    {
        PVSBakeResult result;
        
        if (baking.exchange(true)) {
            result.success = false;
            result.errorMessage = "Baking already in progress";
            return result;
        }

        cancelRequested.store(false);
        
        auto startTime = std::chrono::high_resolution_clock::now();

        // Validate inputs
        if (objects.empty()) {
            result.success = false;
            result.errorMessage = "No objects to bake";
            baking.store(false);
            return result;
        }

        ReportProgress(config, 0.0f, "Initializing PVS grid...");
        InitializeOutputData(config, outData);

        auto computeStart = std::chrono::high_resolution_clock::now();

        // Choose baking method
        switch (config.method) {
            case PVSBakeConfig::Method::DISTANCE:
                BakeDistanceBased(config, outData, result);
                break;
            case PVSBakeConfig::Method::RAYCAST:
                BakeRayCast(config, outData, result);
                break;
            case PVSBakeConfig::Method::CUSTOM:
                BakeCustom(config, outData, result);
                break;
        }

        auto computeEnd = std::chrono::high_resolution_clock::now();

        if (!cancelRequested.load()) {
            result.success = true;
            result.statistics = outData.GetStatistics();
            ReportProgress(config, 1.0f, "Baking complete");
        } else {
            result.success = false;
            result.errorMessage = "Baking cancelled";
        }

        auto endTime = std::chrono::high_resolution_clock::now();
        
        result.totalTimeSeconds = std::chrono::duration<float>(endTime - startTime).count();
        result.computeTimeSeconds = std::chrono::duration<float>(computeEnd - computeStart).count();

        baking.store(false);
        return result;
    }

    void PVSBaker::BakeDistanceBased(
        const PVSBakeConfig &config,
        PVSBakedData &outData,
        PVSBakeResult &result)
    {
        float maxDistSq = config.maxVisibilityDistance * config.maxVisibilityDistance;
        uint32_t totalCells = static_cast<uint32_t>(outData.cells.size());
        uint32_t numObjects = static_cast<uint32_t>(objects.size());

        // Pre-compute object centers for SIMD processing
        std::vector<float> objectCenters(numObjects * 3);
        for (uint32_t i = 0; i < numObjects; ++i) {
            const AABB &bounds = objects[i].bounds;
            objectCenters[i * 3 + 0] = (bounds.min.x + bounds.max.x) * 0.5f;
            objectCenters[i * 3 + 1] = (bounds.min.y + bounds.max.y) * 0.5f;
            objectCenters[i * 3 + 2] = (bounds.min.z + bounds.max.z) * 0.5f;
        }

        // Batch processing for SIMD
        constexpr uint32_t BATCH_SIZE = 64;
        std::vector<float> cellCenters(BATCH_SIZE * 3);
        std::vector<float> distancesSq(BATCH_SIZE);

        for (uint32_t cellID = 0; cellID < totalCells && !cancelRequested.load(); ++cellID) {
            const PVSCell &cell = outData.cells[cellID];

            // Pre-fill cell centers for batch
            for (uint32_t i = 0; i < BATCH_SIZE; ++i) {
                cellCenters[i * 3 + 0] = cell.center.x;
                cellCenters[i * 3 + 1] = cell.center.y;
                cellCenters[i * 3 + 2] = cell.center.z;
            }

            // Process objects in batches
            for (uint32_t batchStart = 0; batchStart < numObjects; batchStart += BATCH_SIZE) {
                uint32_t batchEnd = std::min(batchStart + BATCH_SIZE, numObjects);
                uint32_t batchCount = batchEnd - batchStart;

                // Use SIMD to calculate distances
                simd::DistanceSquaredBatch(
                    cellCenters.data(),
                    objectCenters.data() + batchStart * 3,
                    distancesSq.data(),
                    batchCount
                );

                // Update visibility based on distance
                for (uint32_t i = 0; i < batchCount; ++i) {
                    if (distancesSq[i] <= maxDistSq) {
                        uint32_t objID = batchStart + i;
                        uint32_t wordIndex = objID / 64;
                        uint32_t bitIndex = objID % 64;
                        outData.visibilityData[cellID][wordIndex] |= (1ULL << bitIndex);
                    }
                }
            }

            // Report progress periodically
            if (cellID % 100 == 0) {
                float progress = static_cast<float>(cellID) / static_cast<float>(totalCells);
                ReportProgress(config, progress * 0.9f + 0.1f, 
                    "Computing visibility: " + std::to_string(cellID) + "/" + std::to_string(totalCells));
            }
        }
    }

    void PVSBaker::BakeRayCast(
        const PVSBakeConfig &config,
        PVSBakedData &outData,
        PVSBakeResult &result)
    {
        // Ray-cast based baking would require a scene geometry representation
        // For now, fall back to distance-based with a note in the result
        result.errorMessage = "Ray-cast baking not yet implemented, using distance-based";
        BakeDistanceBased(config, outData, result);
    }

    void PVSBaker::BakeCustom(
        const PVSBakeConfig &config,
        PVSBakedData &outData,
        PVSBakeResult &result)
    {
        if (!config.customTestFunc) {
            result.success = false;
            result.errorMessage = "Custom test function not provided";
            return;
        }

        uint32_t totalCells = static_cast<uint32_t>(outData.cells.size());
        uint32_t numObjects = static_cast<uint32_t>(objects.size());

        for (uint32_t cellID = 0; cellID < totalCells && !cancelRequested.load(); ++cellID) {
            const PVSCell &cell = outData.cells[cellID];

            for (uint32_t objID = 0; objID < numObjects; ++objID) {
                bool visible = config.customTestFunc(
                    cell.bounds,
                    cell.center,
                    objects[objID].bounds,
                    objID
                );

                if (visible) {
                    uint32_t wordIndex = objID / 64;
                    uint32_t bitIndex = objID % 64;
                    outData.visibilityData[cellID][wordIndex] |= (1ULL << bitIndex);
                }
            }

            // Report progress periodically
            if (cellID % 100 == 0) {
                float progress = static_cast<float>(cellID) / static_cast<float>(totalCells);
                ReportProgress(config, progress * 0.9f + 0.1f, 
                    "Computing visibility: " + std::to_string(cellID) + "/" + std::to_string(totalCells));
            }
        }
    }

} // namespace sky
