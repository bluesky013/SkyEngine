//
// Created on 2026/09/22.
//

#pragma once

#include <terrain/TerrainTypes.h>
#include <terrain/TerrainTileKey.h>

#include <core/math/Transform.h>
#include <physics/IPhysicsBackend.h>

#include <unordered_map>

namespace sky::terrain {

    class TerrainSystem;

    // Builds and streams per-tile static heightfield colliders from terrain LOD0 samples. Talks to the
    // engine IPhysicsWorld only; no backend type is referenced.
    class TerrainCollisionLayer {
    public:
        TerrainCollisionLayer() = default;
        ~TerrainCollisionLayer();

        void Setup(TerrainSystem *inSystem, phy::IPhysicsWorld *inWorld);
        void Shutdown();

        // Diffs loaded LOD0 tiles and creates/destroys colliders accordingly.
        void Update();

        uint32_t GetColliderCount() const { return static_cast<uint32_t>(colliders.size()); }

        // Backend-neutral helpers (pure, testable without a physics backend).
        static phy::ShapeDesc BuildHeightField(const TerrainMeta &meta, const float *heights, uint32_t vertexSize);
        static Transform     TileCollisionTransform(const TerrainMeta &meta, const TerrainTileCoord &coord);

    private:
        void CreateCollider(const TerrainTileCoord &coord, const float *heights, uint32_t vertexSize);
        void DestroyCollider(const TerrainTileCoord &coord);

        TerrainSystem       *system = nullptr;
        phy::IPhysicsWorld  *world  = nullptr;

        std::unordered_map<TerrainTileCoord, phy::PhysicsObjectId, TerrainTileHash> colliders;
    };

} // namespace sky::terrain
