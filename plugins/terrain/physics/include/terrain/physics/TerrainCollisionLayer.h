//
// Created on 2026/09/22.
//

#pragma once

#include <terrain/TerrainTypes.h>
#include <terrain/TerrainTileKey.h>

#include <core/math/Transform.h>
#include <physics/PhysicsBase.h>

#include <unordered_map>

namespace sky::phy {
    class PhysicsWorld;
    class CollisionObject;
} // namespace sky::phy

namespace sky::terrain {

    class TerrainSystem;

    // Builds and streams per-tile static heightfield colliders from terrain LOD0 samples.
    class TerrainCollisionLayer {
    public:
        TerrainCollisionLayer() = default;
        ~TerrainCollisionLayer();

        void Setup(TerrainSystem *inSystem, phy::PhysicsWorld *inWorld);
        void Shutdown();

        // Diffs loaded LOD0 tiles and creates/destroys colliders accordingly.
        void Update();

        uint32_t GetColliderCount() const { return static_cast<uint32_t>(colliders.size()); }

        // Backend-neutral helpers (pure, testable without a physics backend).
        static phy::HeightFieldShape BuildHeightField(const TerrainMeta &meta, const float *heights, uint32_t vertexSize);
        static Transform             TileCollisionTransform(const TerrainMeta &meta, const TerrainTileCoord &coord);

    private:
        void CreateCollider(const TerrainTileCoord &coord, const float *heights, uint32_t vertexSize);
        void DestroyCollider(const TerrainTileCoord &coord);

        TerrainSystem     *system = nullptr;
        phy::PhysicsWorld *world  = nullptr;

        std::unordered_map<TerrainTileCoord, phy::CollisionObject *, TerrainTileHash> colliders;
    };

} // namespace sky::terrain
