//
// Created by SkyEngine on 2024/02/15.
//

#include <render/culling/PVSCulling.h>
#include <render/culling/SIMDUtils.h>
#include <render/SceneView.h>
#include <cmath>

namespace sky {

    void PVSCulling::Initialize(const PVSConfig &config)
    {
        pvsData.Initialize(config);
        nextObjectID = 0;
        primitiveToID.clear();
        idToPrimitive.clear();
        idToPrimitive.reserve(config.maxObjects);
        initialized = true;
    }

    void PVSCulling::Clear()
    {
        pvsData.Clear();
        primitiveToID.clear();
        idToPrimitive.clear();
        nextObjectID = 0;
        initialized = false;
    }

    PVSObjectID PVSCulling::RegisterPrimitive(RenderPrimitive *primitive)
    {
        if (primitive == nullptr) {
            return INVALID_PVS_OBJECT;
        }

        // Check if already registered
        auto it = primitiveToID.find(primitive);
        if (it != primitiveToID.end()) {
            return it->second;
        }

        // Check max objects limit
        if (nextObjectID >= pvsData.GetConfig().maxObjects) {
            return INVALID_PVS_OBJECT;
        }

        PVSObjectID objectID = nextObjectID++;
        primitiveToID[primitive] = objectID;

        // Expand idToPrimitive if needed
        if (objectID >= idToPrimitive.size()) {
            idToPrimitive.resize(objectID + 1, nullptr);
        }
        idToPrimitive[objectID] = primitive;

        return objectID;
    }

    void PVSCulling::UnregisterPrimitive(RenderPrimitive *primitive)
    {
        if (primitive == nullptr) {
            return;
        }

        auto it = primitiveToID.find(primitive);
        if (it == primitiveToID.end()) {
            return;
        }

        PVSObjectID objectID = it->second;
        
        // Clear visibility for this object from all cells
        for (uint32_t cellID = 0; cellID < pvsData.GetCellCount(); ++cellID) {
            pvsData.ClearVisible(cellID, objectID);
        }

        // Remove from maps
        primitiveToID.erase(it);
        if (objectID < idToPrimitive.size()) {
            idToPrimitive[objectID] = nullptr;
        }
    }

    PVSObjectID PVSCulling::GetObjectID(RenderPrimitive *primitive) const
    {
        auto it = primitiveToID.find(primitive);
        if (it != primitiveToID.end()) {
            return it->second;
        }
        return INVALID_PVS_OBJECT;
    }

    RenderPrimitive* PVSCulling::GetPrimitive(PVSObjectID objectID) const
    {
        if (objectID < idToPrimitive.size()) {
            return idToPrimitive[objectID];
        }
        return nullptr;
    }

    void PVSCulling::QueryVisiblePrimitives(
        const Vector3 &viewPosition,
        const SceneView *sceneView,
        std::vector<RenderPrimitive*> &result) const
    {
        result.clear();

        PVSCellID cellID = pvsData.GetCellID(viewPosition);
        
        // If outside PVS bounds, fall back to all objects (or frustum culling only)
        if (cellID == INVALID_PVS_CELL) {
            for (RenderPrimitive *primitive : idToPrimitive) {
                if (primitive == nullptr) {
                    continue;
                }
                if (sceneView == nullptr || sceneView->FrustumCulling(primitive->worldBound)) {
                    result.push_back(primitive);
                }
            }
            return;
        }

        const PVSBitSet &visibility = pvsData.GetVisibilitySet(cellID);

        // Iterate through all registered objects and check PVS visibility
        for (PVSObjectID objID = 0; objID < idToPrimitive.size(); ++objID) {
            RenderPrimitive *primitive = idToPrimitive[objID];
            if (primitive == nullptr) {
                continue;
            }

            // Check PVS visibility
            if (!visibility.Test(objID)) {
                continue;
            }

            // Apply frustum culling if scene view is provided
            if (sceneView != nullptr && !sceneView->FrustumCulling(primitive->worldBound)) {
                continue;
            }

            result.push_back(primitive);
        }
    }

    void PVSCulling::QueryPVSVisiblePrimitives(
        const Vector3 &viewPosition,
        std::vector<RenderPrimitive*> &result) const
    {
        QueryVisiblePrimitives(viewPosition, nullptr, result);
    }

    void PVSCulling::QueryVisiblePrimitivesOptimized(
        const Vector3 &viewPosition,
        const SceneView *sceneView,
        std::vector<RenderPrimitive*> &result) const
    {
        result.clear();

        PVSCellID cellID = pvsData.GetCellID(viewPosition);
        
        // If outside PVS bounds, fall back to all objects
        if (cellID == INVALID_PVS_CELL) {
            for (RenderPrimitive *primitive : idToPrimitive) {
                if (primitive == nullptr) {
                    continue;
                }
                if (sceneView == nullptr || sceneView->FrustumCulling(primitive->worldBound)) {
                    result.push_back(primitive);
                }
            }
            return;
        }

        const PVSBitSet &visibility = pvsData.GetVisibilitySet(cellID);
        
        // Pre-reserve based on visible count
        result.reserve(visibility.CountSet());

        // Use fast bit iteration - only visit set bits
        visibility.ForEachSetBit([&](uint32_t objID) {
            if (objID < idToPrimitive.size()) {
                RenderPrimitive *primitive = idToPrimitive[objID];
                if (primitive != nullptr) {
                    // Apply frustum culling if scene view is provided
                    if (sceneView == nullptr || sceneView->FrustumCulling(primitive->worldBound)) {
                        result.push_back(primitive);
                    }
                }
            }
        });
    }

    uint32_t PVSCulling::GetVisibleCount(const Vector3 &viewPosition) const
    {
        PVSCellID cellID = pvsData.GetCellID(viewPosition);
        if (cellID == INVALID_PVS_CELL) {
            return static_cast<uint32_t>(idToPrimitive.size());
        }
        return pvsData.GetVisibilitySet(cellID).CountSet();
    }

    bool PVSCulling::IsPrimitiveVisible(const Vector3 &viewPosition, RenderPrimitive *primitive) const
    {
        PVSObjectID objectID = GetObjectID(primitive);
        if (objectID == INVALID_PVS_OBJECT) {
            return false;
        }

        PVSCellID cellID = pvsData.GetCellID(viewPosition);
        if (cellID == INVALID_PVS_CELL) {
            // Outside PVS bounds - consider visible (conservative)
            return true;
        }

        return pvsData.IsVisible(cellID, objectID);
    }

    void PVSCulling::ComputeVisibility(
        const std::function<bool(PVSCellID, PVSObjectID, const AABB&, const AABB&)> &testFunc)
    {
        if (!initialized) {
            return;
        }

        for (uint32_t cellID = 0; cellID < pvsData.GetCellCount(); ++cellID) {
            const AABB &cellBounds = pvsData.GetCell(cellID).bounds;
            
            for (const auto &pair : primitiveToID) {
                RenderPrimitive *primitive = pair.first;
                PVSObjectID objectID = pair.second;
                
                if (testFunc(cellID, objectID, cellBounds, primitive->worldBound)) {
                    pvsData.SetVisible(cellID, objectID);
                } else {
                    pvsData.ClearVisible(cellID, objectID);
                }
            }
        }
    }

    void PVSCulling::ComputeDistanceBasedVisibility(float maxDistance)
    {
        if (!initialized || primitiveToID.empty()) {
            return;
        }

        float maxDistSq = maxDistance * maxDistance;
        uint32_t numObjects = static_cast<uint32_t>(idToPrimitive.size());
        
        // Prepare object center data for SIMD batch processing
        std::vector<float> objectCenters(numObjects * 3);
        std::vector<bool> objectValid(numObjects, false);
        
        for (const auto &pair : primitiveToID) {
            RenderPrimitive *primitive = pair.first;
            PVSObjectID objectID = pair.second;
            
            if (primitive != nullptr && objectID < numObjects) {
                const AABB &bounds = primitive->worldBound;
                size_t offset = objectID * 3;
                objectCenters[offset + 0] = (bounds.min.x + bounds.max.x) * 0.5f;
                objectCenters[offset + 1] = (bounds.min.y + bounds.max.y) * 0.5f;
                objectCenters[offset + 2] = (bounds.min.z + bounds.max.z) * 0.5f;
                objectValid[objectID] = true;
            }
        }

        // Batch size for SIMD processing
        constexpr uint32_t BATCH_SIZE = 64;
        std::vector<float> cellCenters(BATCH_SIZE * 3);
        std::vector<float> distancesSq(BATCH_SIZE);

        // Process each cell
        for (uint32_t cellID = 0; cellID < pvsData.GetCellCount(); ++cellID) {
            const PVSCell &cell = pvsData.GetCell(cellID);
            float cellCenterX = cell.center.x;
            float cellCenterY = cell.center.y;
            float cellCenterZ = cell.center.z;

            // Pre-fill cell centers for the full batch (reused for all batches in this cell)
            for (uint32_t i = 0; i < BATCH_SIZE; ++i) {
                cellCenters[i * 3 + 0] = cellCenterX;
                cellCenters[i * 3 + 1] = cellCenterY;
                cellCenters[i * 3 + 2] = cellCenterZ;
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
                    PVSObjectID objectID = batchStart + i;
                    if (!objectValid[objectID]) {
                        continue;
                    }

                    if (distancesSq[i] <= maxDistSq) {
                        pvsData.SetVisible(cellID, objectID);
                    } else {
                        pvsData.ClearVisible(cellID, objectID);
                    }
                }
            }
        }
    }

} // namespace sky
