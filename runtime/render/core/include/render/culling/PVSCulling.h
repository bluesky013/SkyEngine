//
// Created by SkyEngine on 2024/02/15.
//

#pragma once

#include <memory>
#include <unordered_map>
#include <functional>
#include <render/culling/PVSData.h>
#include <render/RenderPrimitive.h>
#include <core/shapes/AABB.h>

namespace sky {

    class SceneView;
    class RenderScene;

    /**
     * @brief Manages PVS-based visibility culling for a render scene
     * 
     * The PVSCulling class provides:
     * - Object registration and management
     * - Pre-computed visibility data storage
     * - Runtime visibility queries combining PVS with frustum culling
     * - Visibility computation utilities
     */
    class PVSCulling {
    public:
        PVSCulling() = default;
        ~PVSCulling() = default;

        /**
         * @brief Initialize the PVS culling system
         * @param config PVS configuration
         */
        void Initialize(const PVSConfig &config);

        /**
         * @brief Clear all PVS data and reset the system
         */
        void Clear();

        /**
         * @brief Register a render primitive with the PVS system
         * @param primitive Render primitive to register
         * @return Object ID for use in visibility queries
         */
        PVSObjectID RegisterPrimitive(RenderPrimitive *primitive);

        /**
         * @brief Unregister a render primitive from the PVS system
         * @param primitive Render primitive to unregister
         */
        void UnregisterPrimitive(RenderPrimitive *primitive);

        /**
         * @brief Get the object ID for a registered primitive
         * @param primitive Render primitive
         * @return Object ID or INVALID_PVS_OBJECT if not registered
         */
        PVSObjectID GetObjectID(RenderPrimitive *primitive) const;

        /**
         * @brief Get the primitive for an object ID
         * @param objectID Object ID
         * @return Render primitive or nullptr if not found
         */
        RenderPrimitive* GetPrimitive(PVSObjectID objectID) const;

        /**
         * @brief Query visible primitives from a view position
         * 
         * This combines PVS lookup with optional frustum culling
         * 
         * @param viewPosition Camera/view position
         * @param sceneView Optional scene view for frustum culling
         * @param result Vector to store visible primitives
         */
        void QueryVisiblePrimitives(
            const Vector3 &viewPosition,
            const SceneView *sceneView,
            std::vector<RenderPrimitive*> &result) const;

        /**
         * @brief Query visible primitives using only PVS (no frustum culling)
         * @param viewPosition Camera/view position
         * @param result Vector to store visible primitives
         */
        void QueryPVSVisiblePrimitives(
            const Vector3 &viewPosition,
            std::vector<RenderPrimitive*> &result) const;

        /**
         * @brief Check if a specific primitive is visible from a position
         * @param viewPosition View position
         * @param primitive Primitive to check
         * @return True if potentially visible
         */
        bool IsPrimitiveVisible(const Vector3 &viewPosition, RenderPrimitive *primitive) const;

        /**
         * @brief Get the underlying PVS data for modification
         * @return Reference to PVS data
         */
        PVSData& GetPVSData() { return pvsData; }
        const PVSData& GetPVSData() const { return pvsData; }

        /**
         * @brief Compute visibility for all cells using a visibility test function
         * 
         * The test function should return true if the object is visible from the cell
         * 
         * @param testFunc Function (cellID, objectID, cellBounds, objectBounds) -> bool
         */
        void ComputeVisibility(
            const std::function<bool(PVSCellID, PVSObjectID, const AABB&, const AABB&)> &testFunc);

        /**
         * @brief Compute simple distance-based visibility
         * @param maxDistance Maximum visibility distance
         */
        void ComputeDistanceBasedVisibility(float maxDistance);

        /**
         * @brief Get the number of registered objects
         */
        uint32_t GetObjectCount() const { return static_cast<uint32_t>(primitiveToID.size()); }

        /**
         * @brief Check if the system is initialized
         */
        bool IsInitialized() const { return initialized; }

    private:
        bool initialized = false;
        PVSData pvsData;
        PVSObjectID nextObjectID = 0;

        std::unordered_map<RenderPrimitive*, PVSObjectID> primitiveToID;
        std::vector<RenderPrimitive*> idToPrimitive;
    };

} // namespace sky
