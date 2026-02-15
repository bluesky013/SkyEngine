//
// Created by SkyEngine on 2024/02/15.
//

#include <render/culling/PVSCulling.h>
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
        float maxDistSq = maxDistance * maxDistance;

        ComputeVisibility([maxDistSq](PVSCellID /*cellID*/, PVSObjectID /*objectID*/, 
                                       const AABB &cellBounds, const AABB &objectBounds) {
            // Calculate centers
            Vector3 cellCenter = (cellBounds.min + cellBounds.max) * 0.5f;
            Vector3 objectCenter = (objectBounds.min + objectBounds.max) * 0.5f;

            // Calculate distance squared
            Vector3 diff = objectCenter - cellCenter;
            float distSq = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;

            return distSq <= maxDistSq;
        });
    }

} // namespace sky
