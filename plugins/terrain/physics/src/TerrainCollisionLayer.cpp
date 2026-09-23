//
// Created on 2026/09/22.
//

#include <terrain/physics/TerrainCollisionLayer.h>

#include <terrain/TerrainAddress.h>
#include <terrain/TerrainSystem.h>

#include <physics/CollisionObject.h>
#include <physics/PhysicsRegistry.h>
#include <physics/PhysicsShape.h>
#include <physics/PhysicsWorld.h>

#include <algorithm>
#include <unordered_set>

namespace sky::terrain {

    phy::HeightFieldShape TerrainCollisionLayer::BuildHeightField(const TerrainMeta &meta, const float *heights, uint32_t vertexSize)
    {
        phy::HeightFieldShape shape;
        shape.width       = vertexSize;
        shape.height      = vertexSize;
        shape.samples     = std::vector<float>(heights, heights + static_cast<size_t>(vertexSize) * vertexSize);
        shape.scaleX      = meta.resolution;
        shape.scaleZ      = meta.resolution;
        shape.heightScale = 1.f;
        shape.heightOffset = 0.f;
        shape.upAxis      = 1;

        float minHeight = shape.samples[0];
        float maxHeight = shape.samples[0];
        for (float sample : shape.samples) {
            minHeight = std::min(minHeight, sample);
            maxHeight = std::max(maxHeight, sample);
        }
        shape.minHeight = minHeight;
        shape.maxHeight = maxHeight;
        return shape;
    }

    Transform TerrainCollisionLayer::TileCollisionTransform(const TerrainMeta &meta, const TerrainTileCoord &coord)
    {
        const Vector3 origin = TileToWorld(meta, coord);
        const float   half   = meta.GetTileWorldSize() * 0.5f;

        Transform transform;
        transform.translation = Vector3(origin.x + half, 0.f, origin.z + half);
        return transform;
    }

    TerrainCollisionLayer::~TerrainCollisionLayer()
    {
        Shutdown();
    }

    void TerrainCollisionLayer::Setup(TerrainSystem *inSystem, phy::PhysicsWorld *inWorld)
    {
        system = inSystem;
        world  = inWorld;
    }

    void TerrainCollisionLayer::Shutdown()
    {
        for (auto &entry : colliders) {
            if (world != nullptr) {
                world->RemoveCollisionObject(entry.second);
            }
        }
        colliders.clear();
    }

    void TerrainCollisionLayer::CreateCollider(const TerrainTileCoord &coord, const float *heights, uint32_t vertexSize)
    {
        auto *object = phy::PhysicsRegistry::Get()->CreateCollisionObject();
        if (object == nullptr) {
            return;
        }

        object->SetShape(new phy::PhysicsHeightFieldShape(BuildHeightField(system->GetMeta(), heights, vertexSize)));
        object->SetWorldTransform(TileCollisionTransform(system->GetMeta(), coord));

        world->AddCollisionObject(object);
        colliders.emplace(coord, object);
    }

    void TerrainCollisionLayer::DestroyCollider(const TerrainTileCoord &coord)
    {
        const auto iter = colliders.find(coord);
        if (iter == colliders.end()) {
            return;
        }
        world->RemoveCollisionObject(iter->second);
        colliders.erase(iter);
    }

    void TerrainCollisionLayer::Update()
    {
        if (system == nullptr || world == nullptr) {
            return;
        }

        std::unordered_set<TerrainTileCoord, TerrainTileHash> present;
        for (const auto &key : system->GetLoadedTiles()) {
            if (key.lod == 0) {
                present.insert(key.coord);
            }
        }

        for (auto iter = colliders.begin(); iter != colliders.end();) {
            if (present.count(iter->first) == 0) {
                world->RemoveCollisionObject(iter->second);
                iter = colliders.erase(iter);
            } else {
                ++iter;
            }
        }

        for (const auto &coord : present) {
            if (colliders.count(coord) != 0) {
                continue;
            }
            const float *heights = nullptr;
            uint32_t     size    = 0;
            if (system->GetField().GetTileHeights(coord, heights, size)) {
                CreateCollider(coord, heights, size);
            }
        }
    }

} // namespace sky::terrain
